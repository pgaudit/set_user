/* set-user--3.0.sql */

SET LOCAL search_path to @extschema@;

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION set_user" to load this file. \quit

CREATE FUNCTION @extschema@.set_user(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C;

CREATE FUNCTION @extschema@.set_user(text, text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C STRICT;

REVOKE EXECUTE ON FUNCTION @extschema@.set_user(text) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION @extschema@.set_user(text, text) FROM PUBLIC;

CREATE FUNCTION @extschema@.reset_user()
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C;

CREATE FUNCTION @extschema@.reset_user(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C STRICT;

GRANT EXECUTE ON FUNCTION @extschema@.reset_user() TO PUBLIC;
GRANT EXECUTE ON FUNCTION @extschema@.reset_user(text) TO PUBLIC;

/* New functions in 1.1 (now 1.4) begin here */

CREATE FUNCTION @extschema@.set_user_u(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C STRICT;

REVOKE EXECUTE ON FUNCTION @extschema@.set_user_u(text) FROM PUBLIC;

/* No new sql functions for 1.5 */
/* No new sql functions for 1.6 */
/* No new sql functions for 2.0 */

CREATE FUNCTION @extschema@.set_session_auth(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_session_auth'
LANGUAGE C STRICT;
REVOKE EXECUTE ON FUNCTION @extschema@.set_session_auth(text) FROM PUBLIC;
