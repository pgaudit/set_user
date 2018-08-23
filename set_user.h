#ifndef SET_USER_H
#define SET_USER_H

typedef struct SetUserHooks
{
	void	(*post_set_user) (const char *username);
	void	(*post_reset_user) ();
} SetUserHooks;

#define SET_USER_HOOKS_KEY	"SetUserHooks"

#endif
