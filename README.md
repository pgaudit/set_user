# PostgreSQL set_user Extension Module

## Syntax

```
set_user(text rolename) returns text
set_user(text rolename, text token) returns text
set_user_u(text rolename) returns text
reset_user() returns text
reset_user(text token) returns text
```

## Inputs

```rolename``` is the role to be transitioned to.
```token``` if provided during set_user is saved, and then required to be provided again for reset.

## Requirements

* Add set_user to shared_preload_libraries in postgresql.conf.
* Optionally, the following custom parameters may be set to control their respective commands:
  * set_user.block_alter_system = off (defaults to "on")
  * set_user.block_copy_program = off (defaults to "on")
  * set_user.block_log_statement = off (defaults to "on")
  * set_user.superuser_whitelist = '```<user1>```,```<user2>```,...,```<userN>```' (defaults to '*')

## Description

This PostgreSQL extension allows switching users and optionally privilege escalation with enhanced logging and control. It provides an additional layer of logging and control when unprivileged users must escalate themselves to superuser or object owner roles in order to perform needed maintenance tasks. Specifically, when an allowed user executes ```set_user('rolename')``` or ```set_user_u('rolename')```, several actions occur:

* The current effective user becomes ```rolename```.
* The role transition is logged, with specific notation if ```rolename``` is a superuser.
  while in this state will also get logged.
* If set_user.block_alter_system is set to "on", ```ALTER SYSTEM``` commands will be blocked.
* If set_user.block_copy_program is set to "on", ```COPY PROGRAM``` commands will be blocked.
* If set_user.block_log_statement is set to "on", ```SET log_statement``` and
  variations will be blocked.
* If set_user.block_log_statement is set to "on" and ```rolename``` is a database superuser, the current log_statement setting is changed to "all", meaning every SQL statement executed

Only users with EXECUTE permission on ```set_user_u('rolename')``` may escalate to superuser. Additionally, only users explicitly listed in set_user.superuser_whitelist can escalate to superuser. If set_user.superuser_whitelist is explicitly set to the empty set, '', superuser escalation is blocked for all users. If the whitelist is equal to the wildcard character, '*', all users with EXECUTE permission on ```set_user_u()``` can escalate to superuser. The default value of set_user.superuser_whitelist is '*'.

Additionally, with ```set_user('rolename','token')``` the ```token``` is stored in session lifetime memory.

When finished with required actions as ```rolename```, the ```reset_user()``` function is executed to restore the original user. At that point, these actions occur:

* Role transition is logged.
* log_statement setting is set to its original value.
* Blocked command behaviors return to normal.

If ```set_user```, was provided with a ```token```, then ```reset_user('token')``` must be called instead of ```reset_user()```:
* The provided ```token``` is compared with the stored token.
* If the tokens do not match, or if a ```token``` was provided to ```set_user``` but not ```reset_user```, an ERROR occurs.

The concept is to grant the EXECUTE privilege to the ```set_user()``` and/or ```set_user_u()``` function to otherwise unprivileged postgres users. These users can then transition to other roles, possibly escalating themselves to superuser through use of ```set_user_u()```, when needed to perform specific actions. The optional enhanced logging ensures an audit trail of what actions are taken while privileges are altered to those of the alternate role. Note that superuser escalation is only allowed for the roles listed in set_user.superuser_whitelist. If the whitelist is equal to '*', all roles with GRANT EXECUTE ON ```set_user_u``` can escalate to superuser. This is the default setting of set_user.superuser_whitelist.

Once one or more unprivileged users are able to run ```set_user_u()``` in order to escalate their privileges, the superuser account (normally ```postgres```) can be altered to NOLOGIN, preventing any direct database connection by a superuser which would bypass the enhanced logging.

Naturally for this to work as expected, the PostgreSQL cluster must be audited to ensure there are no other PostgreSQL roles existing which are both superuser and can log in. Additionally there must be no unprivileged PostgreSQL roles which have been granted access to one of the existing superuser roles.

Notes:

If set_user.block_log_statement is set to "off", the log_statement setting is left unchanged.

For the blocking of ```ALTER SYSTEM``` and ```COPY PROGRAM``` to work properly, you must include ```set_user``` in shared_preload_libraries in postgresql.conf and restart PostgreSQL.

Neither```set_user('rolename')``` nor ```set_user_u('rolename')``` may be executed from within an explicit transaction block.

## Caveats

In its current state, this extension cannot prevent ```rolename``` from performing a variety of nefarious or otherwise undesireable actions. However, these actions will be logged providing an audit trail, which could also be used to trigger alerts.

Although this extension compiles and works with all supported versions of PostgreSQL starting with PostgreSQL 9.1, all features are not supported until PostgreSQL 9.4 or higher. The ALTER SYSTEM command does not exist prior to 9.4 and COPY PROGRAM does not exist prior to 9.3.

## TODO

The following changes/enhancements are contemplated:

* Improve regression tests

## Installation

### Requirements

* PostgreSQL 9.1 or higher.

### Compile and Install

Clone PostgreSQL repository:

```bash
$> git clone https://github.com/postgres/postgres.git
```

Checkout REL9_5_STABLE (for example) branch:

```bash
$> git checkout REL9_5_STABLE
```

Make PostgreSQL:

```bash
$> ./configure
$> make install -s
```

Change to the contrib directory:

```bash
$> cd contrib
```

Clone ```set_user``` extension:

```bash
$> git clone https://github.com/pgaudit/set_user
```

Change to ```set_user``` directory:

```bash
$> cd set_user
```

Build ```set_user```:

```bash
$> make
```

Install ```set_user```:

```bash
$> make install
```

#### Using PGXS

If an instance of PostgreSQL is already installed, then PGXS can be utilized to build and install ```set_user```.  Ensure that PostgreSQL binaries are available via the ```$PATH``` environment variable then use the following commands.

```bash
$> make USE_PGXS=1
$> make USE_PGXS=1 install
```

### Configure

The following bash commands should configure your system to utilize set_user. Replace all paths as appropriate. It may be prudent to visually inspect the files afterward to ensure the changes took place.

###### Initialize PostgreSQL (if needed):

```bash
$> initdb -D /path/to/data/directory
```

###### Create Target Database (if needed):

```bash
$> createdb <database>
```

###### Install ```set_user``` functions:

Edit postgresql.conf and add ```set_user``` to the shared_preload_libraries line, optionally also changing custom settings as mentioned above.

First edit postgresql.conf in your favorite editor:

```
$> vi $PGDATA/postgresql.conf
```

Then add these lines to the end of the file:
```
# Add set_user to any existing list
shared_preload_libraries = 'set_user'
# The following lines are only required to modify the
# blocking of each respective command if desired
set_user.block_alter_system = off       #defaults to "on"
set_user.block_copy_program = off       #defaults to "on"
set_user.block_log_statement = off      #defaults to "on"
set_user.superuser_whitelist = ''       #defaults to '*'
```

Finally, restart PostgreSQL (method may vary):

```
$> service postgresql restart
```

Install the extension into your database:

```bash
psql <database>
CREATE EXTENSION set_user;
```


## GUC Parameters

* Block ALTER SYSTEM commands
  * set_user.block_alter_system = on
* Block COPY PROGRAM commands
  * set_user.block_copy_program = on
* Block SET log_statement commands
  * set_user.block_log_statement = on
* Allow list of users to escalate to superuser
  * set_user.superuser_whitelist = '```<user1>,<user2>,...,<userN>```'


## Examples

```
#################################
# OS command line, terminal 1
#################################
psql -U postgres <dbname>

---------------------------------
-- psql command line, terminal 1
---------------------------------
SELECT rolname FROM pg_authid WHERE rolsuper and rolcanlogin;
 rolname
----------
 postgres
(1 row)

CREATE EXTENSION set_user;
CREATE USER dba_user;
GRANT EXECUTE ON FUNCTION set_user(text) TO dba_user;
GRANT EXECUTE ON FUNCTION set_user_u(text) TO dba_user;

#################################
# OS command line, terminal 2
#################################
psql -U dba_user <dbname>

---------------------------------
-- psql command line, terminal 2
---------------------------------
SELECT set_user('postgres');
ERROR:  Switching to superuser only allowed for privileged procedure:
'set_user_u'
SELECT set_user_u('postgres');
SELECT CURRENT_USER, SESSION_USER;
 current_user | session_user
--------------+--------------
 postgres     | dba_user
(1 row)

SELECT reset_user();
SELECT CURRENT_USER, SESSION_USER;
 current_user | session_user
--------------+--------------
 dba_user     | dba_user
(1 row)

\q

---------------------------------
-- psql command line, terminal 1
---------------------------------
ALTER USER postgres NOLOGIN;
-- repeat terminal 2 test with dba_user before exiting
\q

#################################
# OS command line, terminal 1
#################################
tail -n 6 <postgres log>
LOG:  Role dba_user transitioning to Superuser Role postgres
STATEMENT:  SELECT set_user_u('postgres');
LOG:  statement: SELECT CURRENT_USER, SESSION_USER;
LOG:  statement: SELECT reset_user();
LOG:  Superuser Role postgres transitioning to Role dba_user
STATEMENT:  SELECT reset_user();

#################################
# OS command line, terminal 2
#################################
psql -U dba_user <dbname>

---------------------------------
-- psql command line, terminal 2
---------------------------------
-- Verify there are no superusers that can login directly
SELECT rolname FROM pg_authid WHERE rolsuper and rolcanlogin;
 rolname
---------
(0 rows)

-- Verify there are no unprivileged roles that can login directly
-- that are granted a superuser role even if it is multiple layers
-- removed
DROP VIEW IF EXISTS roletree;
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

-- For example purposes, given this set of roles
SELECT r.rolname, r.rolsuper, r.rolinherit,
  r.rolcreaterole, r.rolcreatedb, r.rolcanlogin,
  r.rolconnlimit, r.rolvaliduntil,
  ARRAY(SELECT b.rolname
        FROM pg_catalog.pg_auth_members m
        JOIN pg_catalog.pg_roles b ON (m.roleid = b.oid)
        WHERE m.member = r.oid) as memberof
, r.rolreplication
, r.rolbypassrls
FROM pg_catalog.pg_roles r
ORDER BY 1;
                                    List of roles
 Role name |                         Attributes                         | Member of  
-----------+------------------------------------------------------------+------------
 bob       |                                                            | {}
 dba_user  |                                                            | {su}
 joe       |                                                            | {newbs}
 newbs     | Cannot login                                               | {}
 postgres  | Superuser, Create role, Create DB, Replication, Bypass RLS | {}
 su        | No inheritance, Cannot login                               | {postgres}

-- This query shows current status is not acceptable
-- 1) postgres can login directly
-- 2) dba_user can login and is able to escalate without using set_user()
SELECT
  ro.rolname,
  ro.roloid,
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
 rolname  | roloid | rolcanlogin | rolsuper |  rolparents   
----------+--------+-------------+----------+---------------
 dba_user |  16387 | t           | f        | {postgres,su}
 postgres |     10 | t           | t        | {}
(2 rows)

-- Fix it
REVOKE postgres FROM su;
ALTER USER postgres NOLOGIN;

-- Rerun the query - shows current status is acceptable
SELECT
  ro.rolname,
  ro.roloid,
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
 rolname | roloid | rolcanlogin | rolsuper | rolparents 
---------+--------+-------------+----------+------------
(0 rows)
```

##  Licensing

Please see the [LICENSE](./LICENSE) file.


