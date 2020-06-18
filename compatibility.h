/* -------------------------------------------------------------------------
 *
 * compatibility.h
 *
 * Definitions for maintaining compatibility across Postgres versions.
 *
 * Copyright (c) 2010-2020, PostgreSQL Global Development Group
 *
 * -------------------------------------------------------------------------
 */

#ifndef SET_USER_COMPAT_H
#define SET_USER_COMPAT_H

/*
 * PostgreSQL GUC variable deprecation handling.
 */
#include "miscadmin.h"
#include "utils/guc.h"

/*
 * Report deprecated GUC error messages
 *
 * Copied from guc.c call_*_check_hook and modified with more appropriate error
 * message. This is needed to provide error information without actually aborting
 * FATAL in the GUC check hook.
 */
static inline void
deprecated_guc_errmsg(int elevel, const char *oldname, const char *newname)
{
	/*
	 * Prevent multiple NOTICE messages from ProcessUtilityHook and
	 * only show one LOG error per backend.
	 */
	if (elevel == NOTICE || MyProcPid == PostmasterPid)
	{
		ereport(elevel,
		        (errcode(ERRCODE_CONFIG_FILE_ERROR),
			 GUC_check_errmsg_string ?
			 errmsg_internal("set_user: %s", GUC_check_errmsg_string) :
			 errmsg("set_user: deprecated GUC name \"set_user.%s\"",
				oldname),
			 GUC_check_errdetail_string ?
			 errdetail_internal("%s", GUC_check_errdetail_string) : 0,
			 GUC_check_errhint_string ?
			 errhint("%s", GUC_check_errhint_string) :
			 errhint("use \"set_user.%s\" instead", newname)));
	}
}

#define DEFINE_DEPRECATED_GUC(deprecated_name, new_name, value) \
	DefineCustomStringVariable("set_user." #deprecated_name, \
					"Deprecated variable: use \"set_user." #new_name "\" instead", \
					 NULL, &value, NULL, PGC_SIGHUP, 0, NULL, \
					 assign_deprecated_ ##deprecated_name, \
					 show_deprecated_ ##deprecated_name);

#define DEPRECATED_VARIABLE_NAME(deprecated_name, new_name, value) \
bool  show_ ##deprecated_name## _notice = true; \
static void assign_deprecated_ ##deprecated_name (const char *newval, void *extra) \
{ \
	if (newval) \
	{ \
		GUC_check_errmsg("multiple guc sources"); \
		GUC_check_errdetail("\"set_user.%s\" and \"set_user.%s\" (deprecated) both set", \
				    #new_name, #deprecated_name); \
		GUC_check_errhint("defaulting to last change"); \
		deprecated_guc_errmsg(LOG, #deprecated_name, #new_name); \
	} \
} \
static const char *show_deprecated_ ##deprecated_name(void) \
{ \
	if (show_ ##deprecated_name## _notice) \
	{\
		deprecated_guc_errmsg(NOTICE, #deprecated_name, #new_name); \
		show_ ##deprecated_name## _notice = false; \
	}\
	return value; \
}

/*
 * PostgreSQL version 13+
 *
 * Introduces QueryCompletion struct
 */
#if PG_VERSION_NUM >= 130000

static void PU_hook(PlannedStmt *pstmt, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					QueryEnvironment *queryEnv,
					DestReceiver *dest, QueryCompletion *qc);
#define _PU_HOOK \
	static void PU_hook(PlannedStmt *pstmt, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						QueryEnvironment *queryEnv, \
						DestReceiver *dest, QueryCompletion *qc)

#define _prev_hook \
	prev_hook(pstmt, queryString, context, params, queryEnv, dest, qc)

#define _standard_ProcessUtility \
	standard_ProcessUtility(pstmt, queryString,	context, params, queryEnv, dest, qc)

#endif /* 13+ */

/*
 * PostgreSQL version 12+
 *
 * - Removes OID column
 */
#if PG_VERSION_NUM >= 120000
#define HEAP_TUPLE_GET_OID
static inline Oid
_heap_tuple_get_oid(HeapTuple roleTup)
{
	return ((Form_pg_authid) GETSTRUCT(roleTup))->oid;
}

#endif /* 12+ */

/*
 * PostgreSQL version 10+
 *
 * - Introduces PlannedStmt struct
 * - Introduces varlena.h
 */
#if PG_VERSION_NUM >= 100000
# ifndef _PU_HOOK
static void PU_hook(PlannedStmt *pstmt, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					QueryEnvironment *queryEnv, DestReceiver *dest, char *completionTag);
#define _PU_HOOK \
	static void PU_hook(PlannedStmt *pstmt, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						QueryEnvironment *queryEnv, \
						DestReceiver *dest, char *completionTag)

#define _prev_hook \
	prev_hook(pstmt, queryString, context, params, queryEnv, dest, completionTag)

#define _standard_ProcessUtility \
	standard_ProcessUtility(pstmt, queryString, context, params, queryEnv, dest, completionTag)

# endif

#include "utils/varlena.h"
#define parsetree ((Node *) pstmt->utilityStmt)

#endif /* 10+ */

/*
 * PostgreSQL version 9.5+
 *
 * - Introduces two-argument GetUserNameFromId
 */
#if PG_VERSION_NUM >= 90500
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid, false)
#endif

/*
 * PostgreSQL version 9.4+
 *
 * Lowest supported version.
 */
#if PG_VERSION_NUM >= 90400
# ifndef _PU_HOOK
static void PU_hook(Node *parsetree, const char *queryString,
					ProcessUtilityContext context, ParamListInfo params,
					DestReceiver *dest, char *completionTag);

#define _PU_HOOK \
	static void PU_hook(Node *parsetree, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						DestReceiver *dest, char *completionTag)

#define _prev_hook \
	prev_hook(parsetree, queryString, context, params, dest, completionTag)

#define _standard_ProcessUtility \
	standard_ProcessUtility(parsetree, queryString, context, params, dest, completionTag)
# endif

# ifndef GETUSERNAMEFROMID
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid)
# endif

# ifndef HEAP_TUPLE_GET_OID
static inline Oid
_heap_tuple_get_oid(HeapTuple roleTup)
{
	return HeapTupleGetOid(roleTup);
}
# endif
#endif

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 90400
#error "This extension only builds with PostgreSQL 9.4 or later"
#endif

#endif	/* SET_USER_COMPAT_H */
