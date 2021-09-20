/*
 * set_user.c
 *
 * Joe Conway <joe.conway@crunchydata.com>
 *
 * This code is released under the PostgreSQL license.
 *
 * Copyright 2015-2021 Crunchy Data Solutions, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice
 * and this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL CRUNCHY DATA SOLUTIONS, INC. BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE CRUNCHY DATA SOLUTIONS, INC. HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE CRUNCHY DATA SOLUTIONS, INC. SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE CRUNCHY DATA SOLUTIONS, INC. HAS NO
 * OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */
#include "postgres.h"

#include "pg_config.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_proc.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/rel.h"

#include "set_user.h"

PG_MODULE_MAGIC;

#include "compatibility.h"
#include "deprecated_gucs.h"

#define ALLOWLIST_WILDCARD	"*"
#define SUPERUSER_AUDIT_TAG	"AUDIT"

static char *save_log_statement = NULL;
static Oid save_OldUserId = InvalidOid;
static char *reset_token = NULL;
static ProcessUtility_hook_type prev_hook = NULL;
static object_access_hook_type next_object_access_hook;

static bool Block_AS = false;
static bool Block_CP = false;
static bool Block_LS = false;
static char *SU_Allowlist = NULL;
static char *NOSU_TargetAllowlist = NULL;
static char *SU_AuditTag = NULL;
static bool exit_on_error = true;
static const char *set_config_proc_name = "set_config_by_name";
static List *set_config_oid_cache = NIL;

static void PostSetUserHook(bool is_reset, const char *newuser);

extern Datum set_user(PG_FUNCTION_ARGS);
void _PG_init(void);
void _PG_fini(void);

DEPRECATED_GUC(nosuperuser_target_whitelist, nosuperuser_target_allowlist, NOSU_TargetWhitelist, NOSU_TargetAllowlist)
DEPRECATED_GUC(superuser_whitelist, superuser_allowlist, SU_Whitelist, SU_Allowlist)

/* used to block set_config() */
static void set_user_object_access(ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg);
static void set_user_block_set_config(Oid functionId);
static void set_user_check_proc(HeapTuple procTup, Relation rel);
static void set_user_cache_proc(Oid functionId);

/*
 * check_user_allowlist
 *
 * Check if user is contained by allowlist
 *
 */
static bool
check_user_allowlist(Oid userId, const char *allowlist)
{
	char	   *rawstring = NULL;
	List	   *elemlist;
	ListCell   *l;
	bool		result = false;

	if (allowlist == NULL || allowlist[0] == '\0')
		return false;

	rawstring = pstrdup(allowlist);

	/* Parse string into list of identifiers */
	if (!SplitIdentifierString(rawstring, ',', &elemlist))
	{
		/* syntax error in list */
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid syntax in parameter")));
	}

	/* Allow all users to escalate if allowlist is a solo wildcard character. */
	if (list_length(elemlist) == 1)
	{
		char	   *first_elem = NULL;

		first_elem = (char *) linitial(elemlist);
		if (pg_strcasecmp(first_elem, ALLOWLIST_WILDCARD) == 0)
			return true;
	}

	/*
	 * Check whole allowlist to see if it contains the current username and no
	 * wildcard character. Throw an error if the allowlist contains both.
	 */
	foreach(l, elemlist)
	{
		char	   *elem = (char *) lfirst(l);

		if (elem[0] == '+')
		{
			Oid roleId;
			roleId = get_role_oid(elem + 1, false);
			if (!OidIsValid(roleId))
				result = false;

			/* Check to see if userId is contained by group role in allowlist */
			result = has_privs_of_role(userId, roleId);
		}
		else
		{
			if (pg_strcasecmp(elem, GETUSERNAMEFROMID(userId)) == 0)
				result = true;
			else if(pg_strcasecmp(elem, ALLOWLIST_WILDCARD) == 0)
				/* No explicit usernames intermingled with wildcard. */
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid syntax in parameter"),
						 errhint("Either remove users from set_user.superuser_allowlist "
								 "or remove the wildcard character \"%s\". The allowlist "
								 "cannot contain both.",
								 ALLOWLIST_WILDCARD)));
		}
	}
	return result;
}

/*
 * Similar to SET ROLE but with added logging and some additional
 * control over allowed actions
 *
 */
PG_FUNCTION_INFO_V1(set_user);
Datum
set_user(PG_FUNCTION_ARGS)
{
	bool			argisnull = PG_ARGISNULL(0);
	int				nargs = PG_NARGS();
	HeapTuple		roleTup;
	Oid				OldUserId = GetUserId();
	char		   *olduser = GETUSERNAMEFROMID(OldUserId);
	bool			OldUser_is_superuser = superuser_arg(OldUserId);
	Oid				NewUserId = InvalidOid;
	char		   *newuser = NULL;
	bool			NewUser_is_superuser = false;
	char		   *su = "Superuser ";
	char		   *nsu = "";
	MemoryContext	oldcontext;
	bool			is_reset = false;
	bool			is_token = false;
	bool			is_privileged = false;

	/*
	 * Disallow SET ROLE inside a transaction block. The
	 * semantics are too strange, and I cannot think of a
	 * good use case where it would make sense anyway.
	 * Perhaps one day we will need to rethink this...
	 */
	if (IsTransactionBlock())
		ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("set_user: SET ROLE not allowed within transaction block"),
						errhint("Use SET ROLE outside transaction block instead.")));

	/*
	 * set_user(non_null_arg text)
	 *
	 * Might be set_user(username) but might also be set_user(reset_token).
	 * The former case we need to switch user normally, the latter is a
	 * reset with token provided. We need to determine which one we have.
	 */
	if (nargs == 1 && !argisnull)
	{
		Oid				funcOid = fcinfo->flinfo->fn_oid;
		HeapTuple		procTup;
		Form_pg_proc	procStruct;
		char		   *funcname;

		/* Lookup the pg_proc tuple by Oid */
		procTup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcOid));
		if (!HeapTupleIsValid(procTup))
			elog(ERROR, "cache lookup failed for function %u", funcOid);

		procStruct = (Form_pg_proc) GETSTRUCT(procTup);
		if (!procStruct)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("set_user: function lookup failed for %u", funcOid)));
		}
		else if (!NameStr(procStruct->proname))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("set_user: NULL name for function %u", funcOid)));
		}

		funcname = pstrdup(NameStr(procStruct->proname));
		ReleaseSysCache(procTup);

		if (strcmp(funcname, "reset_user") == 0)
		{
			is_reset = true;
			is_token = true;
		}

		if (strcmp(funcname, "set_user_u") == 0)
			is_privileged = true;
	}
	/*
	 * set_user() or set_user(NULL) ==> always a reset
	 */
	else if (nargs == 0 || (nargs == 1 && argisnull))
		is_reset = true;

	if ((nargs == 1 && !is_reset) || nargs == 2)
	{
		/* we are setting a new user */
		if (save_OldUserId != InvalidOid)
			elog(ERROR, "must reset previous user prior to setting again");

		newuser = text_to_cstring(PG_GETARG_TEXT_PP(0));

		/* with 2 args, the caller wants to specify a reset token */
		if (nargs == 2)
		{
			/* use session lifetime memory */
			oldcontext = MemoryContextSwitchTo(TopMemoryContext);

			/* this should never be NULL but just in case */
			if (PG_ARGISNULL(1))
			{
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("set_user: NULL reset_token not valid")));
			}

			/*capture the reset token */
			reset_token = text_to_cstring(PG_GETARG_TEXT_PP(1));
			MemoryContextSwitchTo(oldcontext);
		}

		/* Look up the username */
		roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(newuser));
		if (!HeapTupleIsValid(roleTup))
			elog(ERROR, "role \"%s\" does not exist", newuser);

		NewUserId = _heap_tuple_get_oid(roleTup, AuthIdRelationId);
		NewUser_is_superuser = ((Form_pg_authid) GETSTRUCT(roleTup))->rolsuper;
		ReleaseSysCache(roleTup);

		if (NewUser_is_superuser)
		{
			if (!is_privileged)
				/* can only escalate with set_user_u */
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("switching to superuser not allowed"),
						 errhint("Use \'set_user_u\' to escalate.")));
			else if (!check_user_allowlist(GetUserId(), SU_Allowlist))
				/* check superuser allowlist*/
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("switching to superuser not allowed"),
						 errhint("Add current user to set_user.superuser_allowlist.")));
		}
		else if(!check_user_allowlist(NewUserId, NOSU_TargetAllowlist))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("switching to role is not allowed"),
					 errhint("Add target role to set_user.nosuperuser_target_allowlist.")));
		}

		/* keep track of original userid and value of log_statement */
		save_OldUserId = OldUserId;
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		save_log_statement = pstrdup(GetConfigOption("log_statement",
													 false, false));
		MemoryContextSwitchTo(oldcontext);

		if (NewUser_is_superuser && Block_LS)
		{
			const char	   *old_log_prefix = GetConfigOption("log_line_prefix", true, false);
			char		   *new_log_prefix = NULL;

			if (old_log_prefix)
				new_log_prefix = psprintf("%s%s: ", old_log_prefix, SU_AuditTag);
			else
				new_log_prefix = pstrdup(SU_AuditTag);

			/*
			 * Force logging of everything if block_log_statement is true
			 * and we are escalating to superuser. If not escalating to superuser the
			 * caller could always set log_statement to all prior to using set_user,
			 * and ensure Block_LS is true.
			 */
			SetConfigOption("log_statement", "all", PGC_SUSET, PGC_S_SESSION);

			/*
			 * Add a custom AUDIT tag to postgresql.conf setting
			 * 'log_line_prefix' so log statements are tagged for easy
			 * filtering.
			 */
			SetConfigOption("log_line_prefix", new_log_prefix, PGC_POSTMASTER, PGC_S_SESSION);
		}
	}
	else if (is_reset)
	{
		char	   *user_supplied_token = NULL;

		/* set_user not active, nothing to do */
		if (save_OldUserId == InvalidOid)
			PG_RETURN_TEXT_P(cstring_to_text("OK"));

		if (reset_token && !is_token)
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("reset token required but not provided")));
		else if (reset_token && is_token)
			user_supplied_token = text_to_cstring(PG_GETARG_TEXT_PP(0));

		if (reset_token)
		{
			if (strcmp(reset_token, user_supplied_token) != 0)
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						errmsg("incorrect reset token provided")));
		}

		/* get original userid to whom we will reset */
		NewUserId = save_OldUserId;
		newuser = GETUSERNAMEFROMID(NewUserId);
		NewUser_is_superuser = superuser_arg(NewUserId);

		/* flag that we are now reset */
		save_OldUserId = InvalidOid;

		/* restore original log_statement setting if block_log_statement is true */
		if (Block_LS)
			SetConfigOption("log_statement", save_log_statement, PGC_SUSET, PGC_S_SESSION);

		pfree(save_log_statement);
		save_log_statement = NULL;

		if (reset_token)
		{
			pfree(reset_token);
			reset_token = NULL;
		}
	}
	else
		/* should not happen */
		elog(ERROR, "unexpected argument combination");

	elog(LOG, "%sRole %s transitioning to %sRole %s",
			  OldUser_is_superuser ? su : nsu,
			  olduser,
			  NewUser_is_superuser ? su : nsu,
			  newuser);

	SetCurrentRoleId(NewUserId, NewUser_is_superuser);
	PostSetUserHook(is_reset, newuser);

	PG_RETURN_TEXT_P(cstring_to_text("OK"));
}

void
_PG_init(void)
{
	DefineCustomBoolVariable("set_user.block_alter_system",
							 "Block ALTER SYSTEM commands",
							 NULL, &Block_AS, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomBoolVariable("set_user.block_copy_program",
							 "Blocks COPY PROGRAM commands",
							 NULL, &Block_CP, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomBoolVariable("set_user.block_log_statement",
							 "Blocks \"SET log_statement\" commands",
							 NULL, &Block_LS, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.nosuperuser_target_allowlist",
							 "List of roles that can be an argument to set_user",
							 NULL, &NOSU_TargetAllowlist, ALLOWLIST_WILDCARD, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.superuser_allowlist",
							 "Allows a list of users to use set_user_u for superuser escalation",
							 NULL, &SU_Allowlist, ALLOWLIST_WILDCARD, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.superuser_audit_tag",
							 "Set custom tag for superuser audit escalation",
							 NULL, &SU_AuditTag, SUPERUSER_AUDIT_TAG, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomBoolVariable("set_user.exit_on_error",
							 "Exit backend process on ERROR during set_session_auth()",
							 NULL, &exit_on_error, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);
	
	DefineDeprecatedStringVariable(nosuperuser_target_whitelist, nosuperuser_target_allowlist, NOSU_TargetWhitelist, NOSU_TargetAllowlist);
	DefineDeprecatedStringVariable(superuser_whitelist, superuser_allowlist, SU_Whitelist, SU_Allowlist);

	/* Install hook */
	prev_hook = ProcessUtility_hook;
	ProcessUtility_hook = PU_hook;

	/* Object access hook */
	next_object_access_hook = object_access_hook;
	object_access_hook = set_user_object_access;
}

void
_PG_fini(void)
{
	ProcessUtility_hook = prev_hook;
}

/*
 * _PU_HOOK
 *
 * Compatibility shim for PU_hook. Handles changing function signature
 * between versions of PostgreSQL.
 */
_PU_HOOK
{
	/* if set_user has been used to transition, enforce set_user GUCs */
	if (save_OldUserId != InvalidOid)
	{
		switch (nodeTag(parsetree))
		{
			case T_AlterSystemStmt:
				if (Block_AS)
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("ALTER SYSTEM blocked by set_user config")));
				break;
			case T_CopyStmt:
				if (((CopyStmt *) parsetree)->is_program && Block_CP)
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("COPY PROGRAM blocked by set_user config")));
				break;
			case T_VariableSetStmt:
				if ((strcmp(((VariableSetStmt *) parsetree)->name,
					 "log_statement") == 0) &&
					Block_LS)
				{
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("\"SET log_statement\" blocked by set_user config")));
				}
				else if ((strcmp(((VariableSetStmt *) parsetree)->name,
					 "role") == 0))
				{
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("\"SET/RESET ROLE\" blocked by set_user"),
							 errhint("Use \"SELECT set_user();\" or \"SELECT reset_user();\" instead.")));
				}
				else if ((strcmp(((VariableSetStmt *) parsetree)->name,
					 "session_authorization") == 0))
				{
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("\"SET/RESET SESSION AUTHORIZATION\" blocked by set_user"),
							 errhint("Use \"SELECT set_user();\" or \"SELECT reset_user();\" instead.")));
				}
				break;
			default:
				break;
		}
	}

	/*
	 * Now pass-off handling either to the previous ProcessUtility hook
	 * or to the standard ProcessUtility.
	 *
	 * These functions are also called by their compatibility variants.
	 */
	if (prev_hook)
	{
		_prev_hook;
	}
	else
	{
		_standard_ProcessUtility;
	}
}

/*
 * PostSetUserHook
 *
 * Handler for set_user post hooks
 */
void
PostSetUserHook(bool is_reset, const char *username)
{
	List		  **hooks_queue;
	ListCell	   *hooks_entry = NULL;

	hooks_queue = (List **) find_rendezvous_variable(SET_USER_HOOKS_KEY);
	foreach (hooks_entry, *hooks_queue)
	{
		SetUserHooks	**post_hooks = (SetUserHooks **) lfirst(hooks_entry);
		if (post_hooks)
		{
			if (!is_reset && (*post_hooks)->post_set_user)
			{
				(*post_hooks)->post_set_user(username);
			}
			else if ((*post_hooks)->post_reset_user)
			{
				(*post_hooks)->post_reset_user();
			}
		}
	}
}

/*
 * Similar to SET SESSION AUTHORIZATION, except:
 * 
 * 1. does not require superuser (GRANTable)
 * 2. does not allow switching to a superuser
 * 3. does not allow reset/switching back
 * 4. Can be configured to throw FATAL/exit for all ERRORs
 */
PG_FUNCTION_INFO_V1(set_session_auth);
Datum
set_session_auth(PG_FUNCTION_ARGS)
{
	bool orig_exit_on_err = ExitOnAnyError;
#if NO_ASSERT_AUTH_UID_ONCE
	char		   *newuser = text_to_cstring(PG_GETARG_TEXT_PP(0));
	HeapTuple		roleTup;
	bool			NewUser_is_superuser = false;

	ExitOnAnyError = exit_on_error;

	/* Look up the username */
	roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(newuser));
	if (!HeapTupleIsValid(roleTup))
		elog(ERROR, "role \"%s\" does not exist", newuser);

	NewUser_is_superuser = ((Form_pg_authid) GETSTRUCT(roleTup))->rolsuper;
	ReleaseSysCache(roleTup);

	/* cannot escalate to superuser */
	if (NewUser_is_superuser)
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("switching to superuser not allowed"),
				 errhint("Use \'set_user_u\' to escalate.")));

	InitializeSessionUserId(newuser, InvalidOid);
#else
	ExitOnAnyError = exit_on_error;
	elog(ERROR, "Assert build disables set_session_auth()");
#endif

	ExitOnAnyError = orig_exit_on_err;
	PG_RETURN_TEXT_P(cstring_to_text("OK"));
}
	
/*
 * set_user_object_access
 *
 * Add some extra checking of bypass functions using the object access hook.
 *
 */
static void
set_user_object_access (ObjectAccessType access, Oid classId, Oid objectId, int subId, void *arg)
{
	/* If set_user has been used to transition, enforce `set_config` block. */
	if (save_OldUserId != InvalidOid)
	{
		if (next_object_access_hook)
		{
			(*next_object_access_hook)(access, classId, objectId, subId, arg);
		}

		switch (access)
		{
			case OAT_FUNCTION_EXECUTE:
			{
				/* Update the `set_config` Oid cache if necessary. */
				set_user_cache_proc(InvalidOid);

				/* Now see if this function is blocked */
				set_user_block_set_config(objectId);
				break;
			}
			case OAT_POST_ALTER:
			case OAT_POST_CREATE:
			{
				if (classId == ProcedureRelationId)
				{
					set_user_cache_proc(objectId);
				}
				break;
			}
			default:
				break;
		}
	}
}

/*
 * set_user_block_set_config
 *
 * Error out if the provided functionId is in the `set_config_procs` cache.
 */
static void
set_user_block_set_config(Oid functionId)
{
	MemoryContext	ctx;

	/* This is where we store the set_config Oid cache. */
	ctx = MemoryContextSwitchTo(CacheMemoryContext);

	/* Check the cache for the current function Oid */
	if (list_member_oid(set_config_oid_cache, functionId))
	{
		ObjectAddress	object;
		char		*funcname = NULL;

		object.classId = ProcedureRelationId;
		object.objectId = functionId;
		object.objectSubId = 0;

		funcname = getObjectIdentity(&object);
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 errmsg("\"%s\" blocked by set_user", funcname),
				 errhint("Use \"SET\" syntax instead.")));
	}

	MemoryContextSwitchTo(ctx);
}

/*
 * set_user_check_proc
 *
 * Check the specified HeapTuple to see if its `prosrc` attribute matches
 * `set_config_by_name`. Update the cache as appropriate:
 *
 * 1) Add to the cache if it's not there but `prosrc` matches.
 *
 * 2) Remove from the cache if it's present and no longer matches.
 */
static void
set_user_check_proc(HeapTuple procTup, Relation rel)
{
	MemoryContext		ctx;
	Datum       		prosrcdatum;
	bool				isnull;
	Oid					procoid;

	/* For function metadata (Oid) */
	procoid = _heap_tuple_get_oid(procTup, ProcedureRelationId);

	/* Figure out the underlying function */
	prosrcdatum = heap_getattr(procTup, Anum_pg_proc_prosrc, RelationGetDescr(rel), &isnull);
	if (isnull)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("set_user: null prosrc for function %u", procoid)));
	}

	/*
	 * The Oid cache is as good as the underlying cache context, so store it
	 * there.
	 */
	ctx = MemoryContextSwitchTo(CacheMemoryContext);

	/* Make sure the Oid cache is up-to-date */
	if (strcmp(TextDatumGetCString(prosrcdatum), set_config_proc_name) == 0)
	{
		set_config_oid_cache = list_append_unique_oid(set_config_oid_cache, procoid);
	}
	else if (list_member_oid(set_config_oid_cache, procoid))
	{
		set_config_oid_cache = list_delete_oid(set_config_oid_cache, procoid);
	}

	MemoryContextSwitchTo(ctx);
}

/*
 * set_user_cache_proc
 *
 * This function has two modes of operation, based on the provided argument:
 *
 * 1) `functionId` is not set (InvalidOid) - scan all procedures to
 * initialize a list of function Oids which call `set_config_by_name()` under the
 * hood.
 *
 * 2) `functionId` is a valid Oid - grab the syscache entry for the provided
 * Oid to inspect `prosrc` attribute and determine whether it should be in the
 * `set_config_oid_cache` list.
 */
static void
set_user_cache_proc(Oid functionId)
{
	HeapTuple		procTup;
	Relation		rel;
	SysScanDesc		sscan;
	/* Defaults for full catalog scan */
	Oid				indexId = InvalidOid;
	bool			indexOk = false;
	Snapshot		snapshot = NULL;
	int				nkeys = 0;
	ScanKeyData		skey;

	/*
	 * If checking the cache for a specific function Oid, we need to narrow the heap
	 * scan by setting a scan key and some other data.
	 */
	if (functionId != InvalidOid)
	{
			indexId = ProcedureOidIndexId;
			indexOk = true;
			snapshot = SnapshotSelf;
			nkeys = 1;
			ScanKeyInit(&skey, Anum_pg_proc_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(functionId));
	}
	else if (set_config_oid_cache != NIL)
	{
		/* No need to re-initialize the cache. We've already been here. */
		return;
	}

	/* Go ahead and do the work */
	PG_TRY();
	{
		rel = table_open(ProcedureRelationId, AccessShareLock);
		sscan = systable_beginscan(rel, indexId, indexOk, snapshot, nkeys, &skey);

		/*
		 * InvalidOid implies complete heap scan to initialize the
		 * set_config cache.
		 *
		 * If we have a scankey, this should only match one item.
		 */
		while (HeapTupleIsValid(procTup = systable_getnext(sscan)))
		{
			set_user_check_proc(procTup, rel);
		}
	}
	PG_CATCH();
	{
		systable_endscan(sscan);
		table_close(rel, NoLock);

		PG_RE_THROW();
	}
	PG_END_TRY();

	systable_endscan(sscan);
	table_close(rel, NoLock);
}
