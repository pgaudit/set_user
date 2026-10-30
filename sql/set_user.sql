CREATE EXTENSION set_user;

-- Ensure the library is loaded.
LOAD 'set_user';

-- Clean up in case a prior regression run failed
-- First suppress NOTICE messages when users/groups don't exist
SET client_min_messages TO 'warning';
DROP USER IF EXISTS dba, bob, joe, newbs, su, "Joe", "Newbs";
RESET client_min_messages;

-- Create some users to work with
CREATE USER dba;
CREATE USER bob;
CREATE USER joe;
CREATE ROLE newbs;
CREATE ROLE su NOINHERIT;

-- dba is the role we want to allow to execute set_user()
GRANT EXECUTE ON FUNCTION set_user(text) TO dba;
GRANT EXECUTE ON FUNCTION set_user(text,text) TO dba;
GRANT EXECUTE ON FUNCTION set_user_u(text) TO dba;
GRANT newbs TO bob;
-- joe will be able to escalate without set_user() via su
GRANT su TO joe;
GRANT postgres TO su;

-- test reset_user with no initial set
SELECT reset_user();

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

-- test multiple successive set_user calls
SELECT set_user('joe'); -- fail

-- ALTER SYSTEM should fail
ALTER SYSTEM SET wal_level = minimal;

-- COPY PROGRAM should fail
COPY (select 42) TO PROGRAM 'cat';

-- SET log_statement should fail
SET log_statement = 'none';
SET log_statement = DEFAULT;
RESET log_statement;
BEGIN; SET LOCAL log_statement = 'none'; ABORT;

-- set_config() should fail
SELECT set_config('wal_level', 'minimal', false);
CREATE OR REPLACE FUNCTION backdoor(text, text, boolean) RETURNS BOOL AS 'set_config_by_name' LANGUAGE INTERNAL;
SELECT backdoor('log_statement', 'none', true);
UPDATE pg_settings SET setting = 'none' WHERE name = 'log_statement';

-- test reset_user
RESET ROLE; -- should fail
RESET SESSION AUTHORIZATION; -- should fail
RESET ALL; -- should fail
SELECT SESSION_USER, CURRENT_USER;

SELECT reset_user();  -- succeed

-- test set_user and reset_user with token
SELECT SESSION_USER, CURRENT_USER;
SELECT set_user('bob', 'secret');
SELECT SESSION_USER, CURRENT_USER;
RESET ROLE; -- should fail
RESET SESSION AUTHORIZATION; -- should fail
DISCARD ALL; -- should fail
DISCARD PLANS; -- succeed
DISCARD SEQUENCES; -- succeed
DISCARD TEMP; -- succeed
SELECT SESSION_USER, CURRENT_USER;

SELECT reset_user(); -- should fail
SELECT reset_user('secreT'); -- should fail
SELECT reset_user('secre'); -- should fail
SELECT reset_user('secrets'); -- should fail
SELECT SESSION_USER, CURRENT_USER;

SELECT reset_user('secret'); -- succeed
SELECT SESSION_USER, CURRENT_USER;

RESET SESSION AUTHORIZATION;
ALTER SYSTEM SET wal_level = minimal;
COPY (select 42) TO PROGRAM 'cat';
SET log_statement = DEFAULT;

-- test transaction handling
CREATE FUNCTION bail() RETURNS bool AS $$
BEGIN
	RAISE EXCEPTION 'bailing out !';
END;
$$ LANGUAGE plpgsql;
SET SESSION AUTHORIZATION dba;
SELECT SESSION_USER, CURRENT_USER;

-- bail during set_user_u
SELECT set_user_u('postgres'), bail();
SELECT SESSION_USER, CURRENT_USER;
SHOW log_statement;
SHOW log_line_prefix;

-- bail on reset after successful set_user_u
SELECT set_user_u('postgres');
SELECT SESSION_USER, CURRENT_USER;
SHOW log_statement;
SHOW log_line_prefix;
SELECT reset_user(), bail();
SELECT SESSION_USER, CURRENT_USER;
SHOW log_statement;
SHOW log_line_prefix;
SELECT reset_user();

-- bail during set_user
SELECT set_user('bob'), bail();
SELECT SESSION_USER, CURRENT_USER;
SHOW log_statement;
SHOW log_line_prefix;

-- bail during set_user with token
SELECT set_user('bob', 'secret'), bail();
SELECT SESSION_USER, CURRENT_USER;
SHOW log_statement;
SHOW log_line_prefix;

-- bail during reset_user with token
SELECT set_user('bob', 'secret');
SELECT SESSION_USER, CURRENT_USER;
SELECT reset_user('secret'), bail();
SELECT SESSION_USER, CURRENT_USER;
SELECT reset_user('secret');

RESET SESSION AUTHORIZATION;

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

-- test statements that are only blocked while escalated
SELECT reset_user();
RESET SESSION AUTHORIZATION;
SELECT SESSION_USER, CURRENT_USER;
RESET ALL; -- succeed
DISCARD ALL; -- succeed

-- test function SET clauses
CREATE FUNCTION set_role_none() RETURNS text LANGUAGE sql
    SET role = 'none' AS $$ SELECT 'ok'::text $$;
CREATE FUNCTION set_search_path() RETURNS text LANGUAGE sql
    SET search_path = 'pg_catalog' AS $$ SELECT 'ok'::text $$;
CREATE FUNCTION security_definer() RETURNS text LANGUAGE sql SECURITY DEFINER
    SET search_path = 'pg_catalog' AS $$ SELECT current_user::text $$;
CREATE FUNCTION call_set_role_none() RETURNS text LANGUAGE plpgsql
    AS $$ BEGIN RETURN set_role_none(); END $$;
CREATE TABLE set_clause_call (id int);
GRANT INSERT ON set_clause_call TO bob;
CREATE FUNCTION trigger_call_set_role_none() RETURNS trigger LANGUAGE plpgsql
    AS $$ BEGIN PERFORM set_role_none(); RETURN NEW; END $$;
CREATE TRIGGER set_clause_call_trigger BEFORE INSERT ON set_clause_call
    FOR EACH ROW EXECUTE FUNCTION trigger_call_set_role_none();
CREATE TABLE set_clause_trigger (id int);
GRANT INSERT ON set_clause_trigger TO bob;
CREATE FUNCTION trigger_set_role_none() RETURNS trigger LANGUAGE plpgsql
    SET role = 'none' AS $$ BEGIN RETURN NEW; END $$;
CREATE TRIGGER set_clause_trigger_trigger BEFORE INSERT ON set_clause_trigger
    FOR EACH ROW EXECUTE FUNCTION trigger_set_role_none();
SELECT set_role_none(); -- succeed
SELECT call_set_role_none(); -- succeed
INSERT INTO set_clause_call VALUES (1); -- succeed
INSERT INTO set_clause_trigger VALUES (1); -- succeed

SET SESSION AUTHORIZATION dba;
SELECT set_user('bob', 'secret');
CREATE FUNCTION pg_temp.f() RETURNS text LANGUAGE sql
    SET role = 'none' AS $$ SELECT current_user::text $$; -- should fail
CREATE FUNCTION pg_temp.f() RETURNS text LANGUAGE sql
    SET "Role" FROM CURRENT AS $$ SELECT current_user::text $$; -- should fail
CREATE FUNCTION pg_temp.f() RETURNS text LANGUAGE sql
    AS $$ SELECT current_user::text $$; -- succeed
ALTER FUNCTION pg_temp.f() SET session_authorization = 'dba'; -- should fail
ALTER FUNCTION pg_temp.f() RESET role; -- succeed
SELECT set_role_none(); -- should fail
SELECT call_set_role_none(); -- should fail, a nested call is checked
INSERT INTO set_clause_call VALUES (2); -- should fail, a call from a trigger is checked
SELECT set_search_path(); -- succeed
SELECT security_definer(); -- succeed, runs as the function owner
SELECT SESSION_USER, CURRENT_USER;

SELECT reset_user('secret'); -- succeed
RESET SESSION AUTHORIZATION;

-- A trigger function with a blocked SET clause is caught by the fmgr hook after
-- the SET clause has been applied so the session is terminated. Run it in a
-- separate session so this one survives.
\setenv PGHOST :HOST
\setenv PGPORT :PORT
\setenv PGUSER :USER
\setenv PGDATABASE :DBNAME
\! psql -X -q -t -A -c "SET SESSION AUTHORIZATION dba" -c "SELECT set_user('bob', 'secret')" -c "INSERT INTO set_clause_trigger VALUES (2)"
SELECT count(*) FROM set_clause_trigger; -- 1, the insert was rolled back

DROP TABLE set_clause_call;
DROP TABLE set_clause_trigger;
DROP FUNCTION pg_temp.f();
DROP FUNCTION trigger_call_set_role_none();
DROP FUNCTION trigger_set_role_none();
DROP FUNCTION call_set_role_none();
DROP FUNCTION set_role_none();
DROP FUNCTION set_search_path();
DROP FUNCTION security_definer();

-- test nosuperuser_target_allowlist
\c -
ALTER SYSTEM SET set_user.nosuperuser_target_allowlist = 'joe,+newbs';
SELECT pg_reload_conf();
\c -
SHOW set_user.nosuperuser_target_allowlist;

SET SESSION AUTHORIZATION dba;
SELECT set_user('joe'); -- succeed, later group entry does not overwrite match
SELECT reset_user();
SELECT set_user('bob'); -- succeed, bob is a member of newbs
SELECT reset_user();
SELECT set_user('su'); -- fail, su is not listed
RESET SESSION AUTHORIZATION;

-- test case-sensitive role names in nosuperuser_target_allowlist
CREATE USER "Joe";
SET SESSION AUTHORIZATION dba;
SELECT set_user('Joe'); -- fail, joe does not match "Joe"
RESET SESSION AUTHORIZATION;

ALTER SYSTEM SET set_user.nosuperuser_target_allowlist = '"Joe"';
SELECT pg_reload_conf();
\c -
SHOW set_user.nosuperuser_target_allowlist;

SET SESSION AUTHORIZATION dba;
SELECT set_user('Joe'); -- succeed
SELECT reset_user();
SELECT set_user('joe'); -- fail, "Joe" does not match joe

-- test case-sensitive group names in nosuperuser_target_allowlist
\c -
CREATE ROLE "Newbs";
GRANT "Newbs" TO joe;
ALTER SYSTEM SET set_user.nosuperuser_target_allowlist = '+"Newbs"';
SELECT pg_reload_conf();
\c -
SHOW set_user.nosuperuser_target_allowlist;

SET SESSION AUTHORIZATION dba;
SELECT set_user('joe'); -- fail, the quotes must enclose the +
RESET SESSION AUTHORIZATION;

ALTER SYSTEM SET set_user.nosuperuser_target_allowlist = '"+Newbs"';
SELECT pg_reload_conf();
\c -
SHOW set_user.nosuperuser_target_allowlist;

SET SESSION AUTHORIZATION dba;
SELECT set_user('joe'); -- succeed, joe is a member of "Newbs"
SELECT reset_user();
SELECT set_user('bob'); -- fail, bob is a member of newbs not "Newbs"

\c -
ALTER SYSTEM RESET set_user.nosuperuser_target_allowlist;
SELECT pg_reload_conf();
\c -
