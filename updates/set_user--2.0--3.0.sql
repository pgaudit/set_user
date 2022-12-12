/* set-user-2.0--3.0.sql */

SET LOCAL search_path to @extschema@;

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION set_user UPDATE to '3.0'" to load this file. \quit

CREATE FUNCTION @extschema@.set_session_auth(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_session_auth'
LANGUAGE C STRICT;

REVOKE EXECUTE ON FUNCTION @extschema@.set_session_auth(text) FROM PUBLIC;
