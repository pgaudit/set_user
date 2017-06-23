/* set-user-1.0--1.1.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION set_user UPDATE to '1.1'" to load this file. \quit

CREATE FUNCTION set_user_u(text)
RETURNS text
AS 'MODULE_PATHNAME', 'set_user'
LANGUAGE C;

REVOKE EXECUTE ON FUNCTION set_user_u(text) FROM PUBLIC;
