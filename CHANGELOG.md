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
