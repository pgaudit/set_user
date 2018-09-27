#ifndef SET_USER_H
#define SET_USER_H

typedef struct SetUserHooks
{
	void	(*post_set_user) (const char *username);
	void	(*post_reset_user) ();
} SetUserHooks;

#define SET_USER_HOOKS_KEY	"SetUserHooks"

/*
 * register_set_user_hooks
 *
 * Utility function for registering an extension's implementation of the
 * set_user hooks.
 *
 * Takes in two function pointers, which should be defined in the extension.
 */
static inline void register_set_user_hooks(void *set_user_hook, void *reset_user_hook)
{
	static SetUserHooks **set_user_hooks = NULL;
	MemoryContext oldcontext;

	/*
	 * Grab the set user hooks from the rendezvous hash. This should always be
	 * NULL, unless some other extension has implemented set_user hooks.
	 */
	set_user_hooks = (SetUserHooks **) find_rendezvous_variable(SET_USER_HOOKS_KEY);

	/*
	 * Populate the hash entry for set_user hooks with extension function
	 * pointers. These hooks should really only be implemented by one
	 * extension, so we'll zero out the entry and override, even if non-NULL.
	 */
	oldcontext = MemoryContextSwitchTo(TopMemoryContext);
	*set_user_hooks = palloc0(sizeof(SetUserHooks));
	(*set_user_hooks)->post_set_user = set_user_hook;
	(*set_user_hooks)->post_reset_user = reset_user_hook;
	MemoryContextSwitchTo(oldcontext);
}

#endif
