/* -------------------------------------------------------------------------
 *
 * deprecated_gucs.h
 *
 * Definitions for deprecated set_user GUCs.
 *
 * Copyright (c) 2010-2021, PostgreSQL Global Development Group
 *
 * -------------------------------------------------------------------------
 */

#ifndef DEPRECATED_GUCS_H
#define DEPRECATED_GUCS_H

/*
 * PostgreSQL GUC variable deprecation handling.
 */
#include "miscadmin.h"
#include "utils/guc.h"

static bool
check_set_user_list(char **newval, void **extra, GucSource source,
			const char *depname, const char *newname, bool *notice)
{
	/*
	 * Notify on the first non-default change for the Postmaster PID only.
	 */
	if (MyProcPid == PostmasterPid && source != PGC_S_DEFAULT && *notice)
	{
		ereport(NOTICE, (errcode(ERRCODE_WARNING_DEPRECATED_FEATURE),
				 errmsg("deprecated GUC: set_user.%s", depname),
				 errhint("Use set_user.%s instead.", newname)));
		*notice = false;
	}

	/*
	 * If the deprecated value is being set, allocate some memory to copy
	 * it to the new GUC, so they remain in sync. Do not free previous
	 * allocated value here, as set_string_field will handle that. A call
	 * to free here would only result in an invalid free in set_string_field.
	 */
	if (*newval)
	{
		*extra = strdup(*newval);
		if (*extra == NULL)
		{
			ereport(FATAL,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("out of memory")));
		}
	}

	return true;
}

#define DefineDeprecatedStringVariable(deprecated_name, new_name, old_value, new_value) \
	DefineCustomStringVariable("set_user." #deprecated_name, \
				   "Deprecated: see \"set_user." #new_name "\"", \
				   NULL, &old_value, ALLOWLIST_WILDCARD, PGC_SIGHUP, GUC_NO_SHOW_ALL, \
				   check_ ##deprecated_name, assign_ ##deprecated_name, show_ ##deprecated_name)


#define DEPRECATED_GUC(deprecated_name, new_name, old_value, new_value) \
static char * old_value; \
static bool notice_ ##old_value = true; \
static bool \
check_ ##deprecated_name(char **newval, void **extra, GucSource source) \
{ \
	return check_set_user_list(newval, extra, source, #deprecated_name, #new_name, &notice_ ##old_value); \
} \
static void \
assign_ ##deprecated_name (const char *newval, void *extra) \
{ \
	if (extra) \
	{ \
		new_value = extra; \
	} \
} \
static const char *show_ ##deprecated_name (void) \
{ \
	return new_value; \
}

#endif /* DEPRECATED_GUCS_H */
