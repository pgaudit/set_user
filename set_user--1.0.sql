/* set-user--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION set_user" to load this file. \quit

CREATE FUNCTION set_user(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C;

CREATE FUNCTION set_user(text, text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C STRICT;

CREATE FUNCTION reset_user()
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C;

CREATE FUNCTION reset_user(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C STRICT;

REVOKE EXECUTE ON FUNCTION set_user(text) FROM PUBLIC;
REVOKE EXECUTE ON FUNCTION set_user(text, text) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION reset_user() TO PUBLIC;
GRANT EXECUTE ON FUNCTION reset_user(text) TO PUBLIC;
