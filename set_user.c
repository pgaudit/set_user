/*
 * set_user.c
 *
 * Similar to SET ROLE but with added logging and some additional
 * control over allowed actions
 *
 * Joe Conway <joe.conway@crunchydata.com>
 *
 * This code is released under the PostgreSQL license.
 *
 * Copyright 2015-2018 Crunchy Data Solutions, Inc.
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

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 90100
/* prior to 9.1 */
#error "This extension only builds with PostgreSQL 9.1 or later"
#elif PG_VERSION_NUM < 90200
/* 9.1 */

#elif PG_VERSION_NUM < 90300
/* 9.2 */

#elif PG_VERSION_NUM < 90400
/* 9.3 */
#define HAS_HTUP_DETAILS
#define HAS_COPY_PROGRAM
#define HAS_PROCESSUTILITYCONTEXT

#elif PG_VERSION_NUM < 90500
/* 9.4 */
#define HAS_HTUP_DETAILS
#define HAS_ALTER_SYSTEM
#define HAS_COPY_PROGRAM
#define HAS_PROCESSUTILITYCONTEXT

#elif PG_VERSION_NUM < 90600
/* 9.5 */
#define HAS_HTUP_DETAILS
#define HAS_ALTER_SYSTEM
#define HAS_COPY_PROGRAM
#define HAS_TWO_ARG_GETUSERNAMEFROMID
#define HAS_PROCESSUTILITYCONTEXT

#elif PG_VERSION_NUM < 100000
/* 9.6 */
#define HAS_HTUP_DETAILS
#define HAS_ALTER_SYSTEM
#define HAS_COPY_PROGRAM
#define HAS_TWO_ARG_GETUSERNAMEFROMID
#define HAS_PROCESSUTILITYCONTEXT

#else
/* master */
#define HAS_HTUP_DETAILS
#define HAS_ALTER_SYSTEM
#define HAS_COPY_PROGRAM
#define HAS_TWO_ARG_GETUSERNAMEFROMID
#define HAS_PROCESSUTILITYCONTEXT
#define HAS_PSTMT
#define HAS_VARLENA_H

#endif

#ifdef HAS_HTUP_DETAILS
#include "access/htup_details.h"
#else
#include "access/htup.h"
#endif

#include "access/xact.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_proc.h"
#include "miscadmin.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#ifdef HAS_VARLENA_H
#include "utils/varlena.h"
#endif /* HAS_VARLENA_H */

#include "set_user.h"
#define WHITELIST_WILDCARD	"*"
#define SUPERUSER_AUDIT_TAG	"AUDIT"

PG_MODULE_MAGIC;

static char *save_log_statement = NULL;
static Oid save_OldUserId = InvalidOid;
static char *reset_token = NULL;
static ProcessUtility_hook_type prev_hook = NULL;

#ifdef HAS_ALTER_SYSTEM
/* 9.4 & up */
static bool Block_AS = false;
#endif

#ifdef HAS_COPY_PROGRAM
/* 9.3 & up */
static bool Block_CP = false;
#endif

static bool Block_LS = false;
static char *SU_Whitelist = NULL;
static char *NOSU_TargetWhitelist = NULL;
static char *SU_AuditTag = NULL;

#ifdef HAS_TWO_ARG_GETUSERNAMEFROMID
/* 9.5 - master */
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid, false)
#else
/* 9.1 - 9.4 */
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid)
#endif

#ifdef HAS_PSTMT
/* 10 & up */
static void PU_hook(PlannedStmt *pstmt, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					QueryEnvironment *queryEnv,
					DestReceiver *dest, char *completionTag);
#else
/* < 10 */
#ifdef HAS_PROCESSUTILITYCONTEXT
/* 9.3 - 9.6 */
static void PU_hook(Node *parsetree, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					DestReceiver *dest, char *completionTag);
#else
/* 9.1 - 9.2 */
static void PU_hook(Node *parsetree, const char *queryString,
					ParamListInfo params, bool isTopLevel,
					DestReceiver *dest, char *completionTag);
#endif
#endif

static void PostSetUserHook(bool is_reset, const char *newuser);

extern Datum set_user(PG_FUNCTION_ARGS);
void _PG_init(void);
void _PG_fini(void);

/*
 * check_user_whitelist
 *
 * Check if user is contained by whitelist
 *
 */
static bool
check_user_whitelist(Oid userId, const char *whitelist)
{
	char	   *rawstring = NULL;
	List	   *elemlist;
	ListCell   *l;
	bool		result = false;

	if (whitelist == NULL || whitelist[0] == '\0')
		return false;

	rawstring = pstrdup(whitelist);

	/* Parse string into list of identifiers */
	if (!SplitIdentifierString(rawstring, ',', &elemlist))
	{
		/* syntax error in list */
		ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
				 errmsg("invalid syntax in parameter")));
	}

	/* Allow all users to escalate if whitelist is a solo wildcard character. */
	if (list_length(elemlist) == 1)
	{
		char	   *first_elem = NULL;

		first_elem = (char *) linitial(elemlist);
		if (pg_strcasecmp(first_elem, WHITELIST_WILDCARD) == 0)
			return true;
	}

	/*
	 * Check whole whitelist to see if it contains the current username and no
	 * wildcard character. Throw an error if the whitelist contains both.
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

			/* Check to see if userId is contained by group role in whitelist */
			result = has_privs_of_role(userId, roleId);
		}
		else
		{
			if (pg_strcasecmp(elem, GETUSERNAMEFROMID(userId)) == 0)
				result = true;
			else if(pg_strcasecmp(elem, WHITELIST_WILDCARD) == 0)
				/* No explicit usernames intermingled with wildcard. */
				ereport(ERROR,
						(errcode(ERRCODE_SYNTAX_ERROR),
						 errmsg("invalid syntax in parameter"),
						 errhint("Either remove users from set_user.superuser_whitelist "
								 "or remove the wildcard character \"%s\". The whitelist "
								 "cannot contain both.",
								 WHITELIST_WILDCARD)));
		}
	}
	return result;
}

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

/* OID column is removed in PG12 */
#if PG_VERSION_NUM >= 120000
		NewUserId = ((Form_pg_authid) GETSTRUCT(roleTup))->oid;
#else
		NewUserId = HeapTupleGetOid(roleTup);
#endif
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
			else if (!check_user_whitelist(GetUserId(), SU_Whitelist))
				/* check superuser whitelist*/
				ereport(ERROR,
						(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
						 errmsg("switching to superuser not allowed"),
						 errhint("Add current user to set_user.superuser_whitelist.")));
		}
		else if(!check_user_whitelist(NewUserId, NOSU_TargetWhitelist))
		{
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 errmsg("switching to role is not allowed"),
					 errhint("Add target role to set_user.nosuperuser_target_whitelist.")));
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
#ifdef HAS_ALTER_SYSTEM
/* 9.4 & up */
	DefineCustomBoolVariable("set_user.block_alter_system",
							 "Block ALTER SYSTEM commands",
							 NULL, &Block_AS, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);
#endif

#ifdef HAS_COPY_PROGRAM
/* 9.3 & up */
	DefineCustomBoolVariable("set_user.block_copy_program",
							 "Blocks COPY PROGRAM commands",
							 NULL, &Block_CP, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);
#endif

	DefineCustomBoolVariable("set_user.block_log_statement",
							 "Blocks \"SET log_statement\" commands",
							 NULL, &Block_LS, true, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.nosuperuser_target_whitelist",
							 "List of roles that can be an argument to set_user",
							 NULL, &NOSU_TargetWhitelist, WHITELIST_WILDCARD, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.superuser_whitelist",
							 "Allows a list of users to use set_user_u for superuser escalation",
							 NULL, &SU_Whitelist, WHITELIST_WILDCARD, PGC_SIGHUP,
							 0, NULL, NULL, NULL);

	DefineCustomStringVariable("set_user.superuser_audit_tag",
							 "Set custom tag for superuser audit escalation",
							 NULL, &SU_AuditTag, SUPERUSER_AUDIT_TAG, PGC_SIGHUP,
							 0, NULL, NULL, NULL);
	/* Install hook */
	prev_hook = ProcessUtility_hook;
	ProcessUtility_hook = PU_hook;
}

void
_PG_fini(void)
{
	ProcessUtility_hook = prev_hook;
}

#ifdef HAS_PSTMT
/* 10 & up */
static void PU_hook(PlannedStmt *pstmt, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					QueryEnvironment *queryEnv,
					DestReceiver *dest, char *completionTag)
#else
/* < 10 */
#ifdef HAS_PROCESSUTILITYCONTEXT
/* 9.3 - 9.6 */
static void
PU_hook(Node *parsetree, const char *queryString,
		ProcessUtilityContext context, ParamListInfo params,
		DestReceiver *dest, char *completionTag)
#else
/* 9.1 - 9.2 */
static void
PU_hook(Node *parsetree, const char *queryString,
		ParamListInfo params, bool isTopLevel,
		DestReceiver *dest, char *completionTag)
#endif
#endif
{

#ifdef HAS_PSTMT
	Node	   *parsetree = pstmt->utilityStmt;
#endif
	/* if set_user has been used to transition, enforce set_user GUCs */
	if (save_OldUserId != InvalidOid)
	{
		switch (nodeTag(parsetree))
		{
#ifdef HAS_ALTER_SYSTEM
/* 9.4 & up */
			case T_AlterSystemStmt:
				if (Block_AS)
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("ALTER SYSTEM blocked by set_user config")));
				break;
#endif

#ifdef HAS_COPY_PROGRAM
/* 9.3 & up */
			case T_CopyStmt:
				if (((CopyStmt *) parsetree)->is_program && Block_CP)
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("COPY PROGRAM blocked by set_user config")));
				break;
#endif

			case T_VariableSetStmt:
				if ((strcmp(((VariableSetStmt *) parsetree)->name,
					 "log_statement") == 0) &&
					Block_LS)
					ereport(ERROR,
							(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
							 errmsg("\"SET log_statement\" blocked by set_user config")));
				break;
			default:
				break;
		}
	}

	/*
	 * Now pass-off handling either to the previous ProcessUtility hook
	 * or to the standard ProcessUtility.
	 */
#ifdef HAS_PSTMT
/* 10 & up */
	if (prev_hook)
		prev_hook(pstmt, queryString, context, params,
				  queryEnv, dest, completionTag);
	else
		standard_ProcessUtility(pstmt, queryString,
								context, params, queryEnv,
								dest, completionTag);
#else
/* < 10 */
#ifdef HAS_PROCESSUTILITYCONTEXT
/* 9.3 & up */
	if (prev_hook)
		prev_hook(parsetree, queryString, context,
				  params, dest, completionTag);
	else
		standard_ProcessUtility(parsetree, queryString,
								context, params,
								dest, completionTag);
#else
/* 9.1 - 9.2 */
	if (prev_hook)
		prev_hook(parsetree, queryString, params,
		isTopLevel, dest, completionTag);
	else
		standard_ProcessUtility(parsetree, queryString,
								params, isTopLevel,
								dest, completionTag);
#endif
#endif
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
