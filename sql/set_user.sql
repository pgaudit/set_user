CREATE EXTENSION set_user;

-- Ensure the library is loaded.
LOAD 'set_user';

-- Clean up in case a prior regression run failed
-- First suppress NOTICE messages when users/groups don't exist
SET client_min_messages TO 'warning';
DROP USER IF EXISTS dba, bob, joe, newbs, su;
RESET client_min_messages;

-- Create some users to work with
CREATE USER dba;
CREATE USER bob;
CREATE USER joe;
CREATE ROLE newbs;
CREATE ROLE su NOINHERIT;

-- dba is the role we want to allow to execute set_user()
GRANT EXECUTE ON FUNCTION set_user(text) TO dba;
GRANT EXECUTE ON FUNCTION set_user_u(text) TO dba;
GRANT newbs TO bob;
-- joe will be able to escalate without set_user() via su
GRANT su TO joe;
GRANT postgres TO su;

-- test set_user
SET SESSION AUTHORIZATION dba;
SELECT SESSION_USER, CURRENT_USER;
SELECT set_user('postgres');
SELECT SESSION_USER, CURRENT_USER;

-- test set_user_u
SET SESSION AUTHORIZATION dba;
SELECT SESSION_USER, CURRENT_USER;
SELECT set_user_u('postgres');
SELECT SESSION_USER, CURRENT_USER;

-- ALTER SYSTEM should fail
ALTER SYSTEM SET wal_level = minimal;

-- COPY PROGRAM should fail
COPY (select 42) TO PROGRAM 'cat';

-- SET log_statement should fail
SET log_statement = 'none';
SET log_statement = DEFAULT;
RESET log_statement;
BEGIN; SET LOCAL log_statement = 'none'; ABORT;

-- test reset_user
SELECT reset_user();
SELECT SESSION_USER, CURRENT_USER;

RESET SESSION AUTHORIZATION;
ALTER SYSTEM SET wal_level = minimal;
COPY (select 42) TO PROGRAM 'cat';
SET log_statement = DEFAULT;

-- this is an example of how we might audit existing roles
SET SESSION AUTHORIZATION dba;
SELECT set_user_u('postgres');
SELECT rolname FROM pg_authid WHERE rolsuper and rolcanlogin;
CREATE OR REPLACE VIEW roletree AS
WITH RECURSIVE
roltree AS (
  SELECT u.rolname AS rolname,
         u.oid AS roloid,
         u.rolcanlogin,
         u.rolsuper,
         '{}'::name[] AS rolparents,
         NULL::oid AS parent_roloid,
         NULL::name AS parent_rolname
  FROM pg_catalog.pg_authid u
  LEFT JOIN pg_catalog.pg_auth_members m on u.oid = m.member
  LEFT JOIN pg_catalog.pg_authid g on m.roleid = g.oid
  WHERE g.oid IS NULL
  UNION ALL
  SELECT u.rolname AS rolname,
         u.oid AS roloid,
         u.rolcanlogin,
         u.rolsuper,
         t.rolparents || g.rolname AS rolparents,
         g.oid AS parent_roloid,
         g.rolname AS parent_rolname
  FROM pg_catalog.pg_authid u
  JOIN pg_catalog.pg_auth_members m on u.oid = m.member
  JOIN pg_catalog.pg_authid g on m.roleid = g.oid
  JOIN roltree t on t.roloid = g.oid
)
SELECT
  r.rolname,
  r.roloid,
  r.rolcanlogin,
  r.rolsuper,
  r.rolparents
FROM roltree r
ORDER BY 1;

-- this will show unacceptable results
-- since postgres can log in directly and
-- joe can escalate via su to postgres
SELECT
  ro.rolname,
  ro.rolcanlogin,
  ro.rolsuper,
  ro.rolparents
FROM roletree ro
WHERE (ro.rolcanlogin AND ro.rolsuper)
OR
(
    ro.rolcanlogin AND EXISTS
    (
      SELECT TRUE FROM roletree ri
      WHERE ri.rolname = ANY (ro.rolparents)
      AND ri.rolsuper
    )
);

-- here is how we fix the environment
-- running this in a transaction that will be aborted
-- since we don't really want to make the postgres user
-- nologin during regression testing
BEGIN;
REVOKE postgres FROM su;
ALTER USER postgres NOLOGIN;

-- retest, this time successfully
SELECT
  ro.rolname,
  ro.rolcanlogin,
  ro.rolsuper,
  ro.rolparents
FROM roletree ro
WHERE (ro.rolcanlogin AND ro.rolsuper)
OR
(
    ro.rolcanlogin AND EXISTS
    (
      SELECT TRUE FROM roletree ri
      WHERE ri.rolname = ANY (ro.rolparents)
      AND ri.rolsuper
    )
);

-- undo those changes
ABORT;
