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
 * Define timingsafe_bcmp() for versions where it does not exist. A distinct
 * name is used to avoid conflicting with a libc version of the function.
 *
 * Copied from PostgreSQL src/port/timingsafe_bcmp.c, which is based on
 * $OpenBSD: timingsafe_bcmp.c,v 1.3 2015/08/31 02:53:57 guenther Exp $
 *
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#if PG_VERSION_NUM < 180000

static inline int
set_user_timingsafe_bcmp(const void *b1, const void *b2, size_t n)
{
	const unsigned char *p1 = b1;
	const unsigned char *p2 = b2;
	int			ret = 0;

	for (; n > 0; n--)
		ret |= *p1++ ^ *p2++;

	return (ret != 0);
}

#define timingsafe_bcmp(b1, b2, n) set_user_timingsafe_bcmp(b1, b2, n)

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
