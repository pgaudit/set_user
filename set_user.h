#ifndef SET_USER_H
#define SET_USER_H

/* Expose a hook for other extensions to use after reset_user finishes up. */
typedef void (*post_reset_user_hook_type) (void);
extern PGDLLIMPORT post_reset_user_hook_type post_reset_user_hook;

/* Expose a hook for other extensions to use after set_user finishes up. */
typedef void (*post_set_user_hook_type) (const char *username);
extern PGDLLIMPORT post_set_user_hook_type post_set_user_hook;
#endif
