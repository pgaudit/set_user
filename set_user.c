/*
 * set_user.c
 *
 * Similar to SET ROLE but with added logging
 * Joe Conway <mail@joeconway.com>
 *
 * Copyright (c) 2002-2015, PostgreSQL Global Development Group
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written agreement
 * is hereby granted, provided that the above copyright notice and this
 * paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS
 * DOCUMENTATION, EVEN IF THE AUTHOR OR DISTRIBUTORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 */
#include "postgres.h"

#include "pg_config.h"
#if PG_VERSION_NUM < 90300
#include "access/htup.h"
#else
#include "access/htup_details.h"
#endif

#include "catalog/pg_authid.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

PG_MODULE_MAGIC;

char   *save_log_statement = NULL;
Oid		save_OldUserId = InvalidOid;

extern Datum set_user(PG_FUNCTION_ARGS);

#if !defined(PG_VERSION_NUM) || PG_VERSION_NUM < 90100
/* prior to 9.1 */
#error "This extension only builds with PostgreSQL 9.1 or later"
#elif PG_VERSION_NUM < 90300
/* 9.1 & 9.2 */
#define GETCONFIGOPTIONBYNAME(cname) GetConfigOptionByName(cname, NULL)
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid)
#elif PG_VERSION_NUM < 90500
/* 9.3 & 9.4 */
#define GETCONFIGOPTIONBYNAME(cname) GetConfigOptionByName(cname, NULL)
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid)
#elif PG_VERSION_NUM < 90600
/* 9.5 */
#define GETCONFIGOPTIONBYNAME(cname) GetConfigOptionByName(cname, NULL)
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid, false)
#else
/* master */
#define GETCONFIGOPTIONBYNAME(cname) GetConfigOptionByName(cname, NULL, false)
#define GETUSERNAMEFROMID(ouserid) GetUserNameFromId(ouserid, false)
#endif

PG_FUNCTION_INFO_V1(set_user);
Datum
set_user(PG_FUNCTION_ARGS)
{
	bool			argisnull = PG_ARGISNULL(0);
	HeapTuple		roleTup;
	Oid				OldUserId = GetUserId();
	char		   *olduser = GETUSERNAMEFROMID(OldUserId);
	bool			OldUser_is_superuser = superuser_arg(OldUserId);
	Oid				NewUserId;
	char		   *newuser;
	bool			NewUser_is_superuser;
	char		   *su = "Superuser ";
	char		   *nsu = "";
	MemoryContext	oldcontext;

	if (!argisnull)
	{
		if (save_OldUserId != InvalidOid)
			elog(ERROR, "must reset previous user prior to setting again");

		newuser = text_to_cstring(PG_GETARG_TEXT_PP(0));

		/* Look up the username */
		roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(newuser));
		if (!HeapTupleIsValid(roleTup))
			elog(ERROR, "role \"%s\" does not exist", newuser);

		NewUserId = HeapTupleGetOid(roleTup);
		NewUser_is_superuser = ((Form_pg_authid) GETSTRUCT(roleTup))->rolsuper;
		ReleaseSysCache(roleTup);

		/* keep track of original userid and value of log_statement */
		save_OldUserId = OldUserId;
		oldcontext = MemoryContextSwitchTo(TopMemoryContext);
		save_log_statement = GETCONFIGOPTIONBYNAME("log_statement");
		MemoryContextSwitchTo(oldcontext);

		/* force logging of everything, at least initially */
		SetConfigOption("log_statement", "all", PGC_SUSET, PGC_S_SESSION);
	}
	else
	{
		if (save_OldUserId == InvalidOid)
			elog(ERROR, "must set user prior to resetting");

		/* get original userid to whom we will reset */
		NewUserId = save_OldUserId;
		newuser = GETUSERNAMEFROMID(NewUserId);
		NewUser_is_superuser = superuser_arg(NewUserId);

		/* flag that we are now reset */
		save_OldUserId = InvalidOid;

		/* restore original log_statement setting */
		SetConfigOption("log_statement", save_log_statement, PGC_SUSET, PGC_S_SESSION);

		pfree(save_log_statement);
		save_log_statement = NULL;
	}

	elog(LOG, "%sRole %s transitioning to %sRole %s",
			  OldUser_is_superuser ? su : nsu,
			  olduser,
			  NewUser_is_superuser ? su : nsu,
			  newuser);

	SetCurrentRoleId(NewUserId, NewUser_is_superuser);

	PG_RETURN_TEXT_P(cstring_to_text("OK"));
}
