4.3.0
=====

SECURITY FIXES
--------------
- Fix a backend crash when `RESET ALL` is run while `set_user()` is active. The crash caused the postmaster to restart all sessions.
- Block `DISCARD ALL` while `set_user()` is active. `DISCARD ALL` reverted the session to the original role without the reset token and without logging the role transition. Call `reset_user()` before `DISCARD ALL`.
- Block function `SET` clauses for `role` and `session_authorization` (and `log_statement` when `set_user.block_log_statement` is on) while `set_user()` is active. These functions reverted the session to the original role without the reset token and without logging the role transition. These functions can no longer be created, altered or called while `set_user()` is active and calling one from a trigger terminates the session.
- Match parameter names case-insensitively when blocking `SET` and `RESET`. Quoted names such as `SET "Role"` bypassed the block.

BUGFIXES
--------
- Fix allowlist matching so a group entry no longer overrides an earlier match.
- Match role names in allowlists case-sensitively. Mixed-case role names must now be quoted, e.g. `'"Alice"'`.
- Compare reset tokens in constant time.

NEW FEATURES
------------
- Remove support for PostgreSQL 13.
- Document the limits of the reset token.

4.2.0
=====

NEW FEATURES
------------
- Add PG_MODULE_MAGIC_EXT() macro for PostgreSQL >= 18.
- Remove support for PostgreSQL 12.

4.1.0
=====

NEW FEATURES
------------
- Add PostgreSQL 17 support.
- Remove support for PostgreSQL < 12.

4.0.1
=====

NEW FEATURES
------------
- Reorganized repository structure to allow for easier management of extension files during build process.
- Added NO_PGXS build flag to allow building of extension without PGXS. Restores ability to build on Windows.
- No changes to extension code.

2.0.1
=====

NEW FEATURES
------------
- Deprecated GUCs are removed from `SHOW ALL`.

BUGFIXES
--------
- NOTICE fixed to only display on first reference to non-default deprecated variable.

2.0.0
=====

NEW FEATURES
------------
- Use of GUCs with `whitelist` have been deprecated in lieu of a more appropriate `allowlist`. The last GUC set by `ALTER SYSTEM` will be used on reload, the first attempt to `SHOW` a deprecated variable will provide a NOTICE.
- The extension is now non-relocatable and all functions are schema-qualified.
