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
