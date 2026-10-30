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
 * Remaining supported versions
 */
#ifndef INITSESSIONUSER
#define INITSESSIONUSER
#define _InitializeSessionUserId(name,ouserid) InitializeSessionUserId(name,ouserid)
#endif

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 140000
#error "This extension only builds with PostgreSQL 14 or later"
#endif

#endif	/* SET_USER_COMPAT_H */
