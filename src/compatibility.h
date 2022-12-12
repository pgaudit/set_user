/* -------------------------------------------------------------------------
 *
 * compatibility.h
 *
 * Definitions for maintaining compatibility across Postgres versions.
 *
 * Copyright (c) 2010-2022, PostgreSQL Global Development Group
 *
 * -------------------------------------------------------------------------
 */

#ifndef SET_USER_COMPAT_H
#define SET_USER_COMPAT_H

#ifndef NO_ASSERT_AUTH_UID_ONCE
#define NO_ASSERT_AUTH_UID_ONCE !USE_ASSERT_CHECKING
#endif

/*
 * PostgreSQL version 14+
 *
 * Introduces ReadOnlyTree boolean
 */
#if PG_VERSION_NUM >= 140000
#define _PU_HOOK \
	static void PU_hook(PlannedStmt *pstmt, const char *queryString, bool ReadOnlyTree, \
						ProcessUtilityContext context, ParamListInfo params, \
						QueryEnvironment *queryEnv, \
						DestReceiver *dest, QueryCompletion *qc)

#define _prev_hook \
	prev_hook(pstmt, queryString, ReadOnlyTree, context, params, queryEnv, dest, qc)

#define _standard_ProcessUtility \
	standard_ProcessUtility(pstmt, queryString, ReadOnlyTree, context, params, queryEnv, dest, qc)

#define getObjectIdentity(address) \
	getObjectIdentity(address,false)

#endif /* 14+ */

/*
 * PostgreSQL version 13+
 *
 * Introduces QueryCompletion struct
 */
#if PG_VERSION_NUM >= 130000
#ifndef _PU_HOOK
#define _PU_HOOK \
	static void PU_hook(PlannedStmt *pstmt, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						QueryEnvironment *queryEnv, \
						DestReceiver *dest, QueryCompletion *qc)

#define _prev_hook \
	prev_hook(pstmt, queryString, context, params, queryEnv, dest, qc)

#define _standard_ProcessUtility \
	standard_ProcessUtility(pstmt, queryString,	context, params, queryEnv, dest, qc)
#endif

#define TABLEOPEN

#endif /* 13+ */

/*
 * PostgreSQL version 12+
 *
 * - Removes OID column
 */
#if PG_VERSION_NUM >= 120000
#define HEAP_TUPLE_GET_OID

/*
 * _heap_tuple_get_oid
 *
 * Return the oid of the tuple based on the provided catalogID.
 */
static inline Oid
_heap_tuple_get_oid(HeapTuple tuple, Oid catalogID)
{
        switch (catalogID)
        {
                case ProcedureRelationId:
                        return ((Form_pg_proc) GETSTRUCT(tuple))->oid;
                        break;
                case AuthIdRelationId:
                        return ((Form_pg_authid) GETSTRUCT(tuple))->oid;
                        break;
                default:
                        ereport(ERROR,
                                        (errcode(ERRCODE_SYNTAX_ERROR),
                                         errmsg("set_user: invalid relation ID provided")));
                        return 0;
        }
}


#include "access/table.h"
#define OBJECTADDRESS
#endif /* 12+ */

/*
 * PostgreSQL version 10+
 *
 * - Introduces PlannedStmt struct
 * - Introduces varlena.h
 */
#if PG_VERSION_NUM >= 100000
#ifndef _PU_HOOK
#define _PU_HOOK \
	static void PU_hook(PlannedStmt *pstmt, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						QueryEnvironment *queryEnv, \
						DestReceiver *dest, char *completionTag)

#define _prev_hook \
	prev_hook(pstmt, queryString, context, params, queryEnv, dest, completionTag)

#define _standard_ProcessUtility \
	standard_ProcessUtility(pstmt, queryString, context, params, queryEnv, dest, completionTag)

#endif

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

#define INITSESSIONUSER
#define _InitializeSessionUserId(name,ouserid) InitializeSessionUserId(name,ouserid)

#endif /* 9.5+ */

/*
 * PostgreSQL version 9.4+
 *
 * Lowest supported version.
 */
#if PG_VERSION_NUM >= 90400
#ifndef _PU_HOOK
#define _PU_HOOK \
	static void PU_hook(Node *parsetree, const char *queryString, \
						ProcessUtilityContext context, ParamListInfo params, \
						DestReceiver *dest, char *completionTag)

#define _prev_hook \
	prev_hook(parsetree, queryString, context, params, dest, completionTag)

#define _standard_ProcessUtility \
	standard_ProcessUtility(parsetree, queryString, context, params, dest, completionTag)
#endif

#ifndef GETUSERNAMEFROMID
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid)
#endif

# ifndef HEAP_TUPLE_GET_OID
static inline Oid
_heap_tuple_get_oid(HeapTuple tup, Oid catalogId)
{
	return HeapTupleGetOid(tup);
}
# endif

#ifndef TABLEOPEN
#define table_open(r, l)	heap_open(r, l)
#define table_close(r, l)	heap_close(r, l)
#endif

#include "access/heapam.h"

#ifndef OBJECTADDRESS
#include "utils/tqual.h"
#endif

#ifndef Anum_pg_proc_oid
#include "access/sysattr.h"
#define Anum_pg_proc_oid ObjectIdAttributeNumber
#define Anum_pg_authid_oid ObjectIdAttributeNumber
#endif

/*
 * _scan_key_init
 *
 * Initialize entry based on the catalogID provided.
 */
static inline void
_scan_key_init(ScanKey entry,
            Oid catalogID,
            StrategyNumber strategy,
            RegProcedure procedure,
            Datum argument)
{
        switch (catalogID)
        {
                case ProcedureRelationId:
                        ScanKeyInit(entry, Anum_pg_proc_oid, strategy, procedure, argument);
                        break;
                default:
                        ereport(ERROR,
                                        (errcode(ERRCODE_SYNTAX_ERROR),
                                         errmsg("set_user: invalid relation ID provided")));
        }
}

#ifndef INITSESSIONUSER
#define _InitializeSessionUserId(name,ouserid) InitializeSessionUserId(name)
#endif

#endif /* 9.4 */

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 90400
#error "This extension only builds with PostgreSQL 9.4 or later"
#endif

/* Use our version-specific static declaration here */
_PU_HOOK;

#endif	/* SET_USER_COMPAT_H */
