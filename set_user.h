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
 * Each subsequent call to this function adds a new hook to the queue.
 */
static inline void register_set_user_hooks(void *set_user_hook, void *reset_user_hook)
{
	static List			  **HooksQueue;
	static SetUserHooks	   *next_hook_entry = NULL;
	MemoryContext			oldcontext;

	oldcontext = MemoryContextSwitchTo(TopMemoryContext);

	/* Grab the SetUserHooks queue from the rendezvous hash */
	HooksQueue = (List **) find_rendezvous_variable(SET_USER_HOOKS_KEY);

	/* Populate a new hooks entry and append it to the queue */
	next_hook_entry = palloc0(sizeof(SetUserHooks));
	next_hook_entry->post_set_user = set_user_hook;
	next_hook_entry->post_reset_user = reset_user_hook;

	*HooksQueue = lappend(*HooksQueue, &next_hook_entry);
	MemoryContextSwitchTo(oldcontext);
}

#endif
