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
 * Define PG_MODULE_MAGIC_EXT() as PG_MODULE_MAGIC for versions where the new
 * macro does not exist
 */
 #if PG_VERSION_NUM < 180000

 #define PG_MODULE_MAGIC_EXT(...) PG_MODULE_MAGIC

 #endif /* PG_VERSION_NUM < 180000 */

/*
 * PostgreSQL version 17+
 *
 * - Sets bypass_login_check parameter to false in InitializeSessionUserId funcion
 */
#if PG_VERSION_NUM >= 170000

#ifndef INITSESSIONUSER
#define INITSESSIONUSER
#define _InitializeSessionUserId(name,ouserid) InitializeSessionUserId(name,ouserid,false)
#endif

#endif /* 17+ */

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

#ifndef INITSESSIONUSER
#define INITSESSIONUSER
#define _InitializeSessionUserId(name,ouserid) InitializeSessionUserId(name,ouserid)

#endif

#endif /* 13+ */

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 130000
#error "This extension only builds with PostgreSQL 13 or later"
#endif

/* Use our version-specific static declaration here */
_PU_HOOK;

#endif	/* SET_USER_COMPAT_H */
