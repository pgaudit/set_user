# PostgreSQL set_user Extension Module

## Syntax

```
set_user(text rolename) returns text
reset_user() returns text
```

## Inputs

```rolename``` is the role to be transitioned to.

## Requirements

* Add set_user to shared_preload_libraries in postgresql.conf.
* Optionally, the following custom parameters may be set to control their respective commands:
** set_user.block_alter_system = on
** set_user.block_copy_program = on
** set_user.block_log_statement = on

## Description

This PostgreSQL extension allows privilege escalation with enhanced logging and
control. It provides an additional layer of logging and control when unprivileged
users must escalate themselves to superuser or object owner roles in order to
perform needed maintenance tasks. Specifically, when an allowed user executes
```set_user('rolename')```, several actions occur:

* The current effective user becomes ```rolename```.
* The role transition is logged, with specific notation if ```rolename``` is a superuser.
* log_statement setting is set to "all", meaning every SQL statement executed while in this state will also get logged.
* If set_user.block_alter_system has been set to "on" in postgresql.conf, ```ALTER SYSTEM``` commands will be blocked.
* If set_user.block_copy_program has been set to "on" in postgresql.conf, ```COPY PROGRAM``` commands will be blocked.
* If set_user.block_log_statement has been set to "on" in postgresql.conf, ```SET log_statement``` and variations will be blocked.

When finished with required actions as ```rolename```, the ```reset_user()``` function
is executed to restore the original user. At that point, these actions occur:

* Role transition is logged.
* log_statement setting is set to its original value.
* Blocked command behaviors return to normal.

The concept is to grant the EXECUTE privilege to the ```set_user()```
function to otherwise unprivileged postgres users who can then
escalate themselves when needed to perform specific superuser actions.
The enhanced logging ensures an audit trail of what actions are taken
while privileges are escalated.

Once one or more unprivileged users are able to run ```set_user()```,
the superuser account (normally ```postgres```) can be altered to NOLOGIN,
preventing any direct database connection by a superuser which would
bypass the enhanced logging.

Naturally for this to work as expected, the PostgreSQL cluster must
be audited to ensure there are no other PostgreSQL roles existing which
are both superuser and can log in. Additionally there must be no
unprivileged PostgreSQL roles which have been granted access to one of
the existing superuser roles.

Note that for the blocking of ```ALTER SYSTEM``` and ```COPY PROGRAM```
to work properly, you must include ```set_user``` in shared_preload_libraries
in postgresql.conf and restart PostgreSQL.

## Caveats

In its current state, this extension cannot prevent ```rolename``` from
performing a variety of nefarious or otherwise undesireable actions.
However, these actions will be logged providing an audit trail, which
could also be used to trigger alerts.

Although this extension compiles and works with all supported versions
of PostgreSQL starting with PostgreSQL 9.1, all features are not supported
until PostgreSQL 9.4 or higher. The ALTER SYSTEM command does not
exist prior to 9.4 and COPY PROGRAM does not exist prior to 9.3.

## TODO

The following changes/enhancements are contemplated:

* Improve regression tests

## Latest Version

Details of the latest version can be acquired by contacting Crunchy
Data Solutions, Inc. (see Contact information below)

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
$> git clone https://github.com/crunchydata/set_user
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

If an instance of PostgreSQL is already installed, then PGXS can be utilized
to build and install ```set_user```.  Ensure that PostgreSQL
binaries are available via the ```$PATH``` environment variable then use the
following commands.

```bash
$> make USE_PGXS=1
$> make USE_PGXS=1 install
```

### Configure

The following bash commands should configure your system to utilize
set_user. Replace all paths as appropriate. It may be prudent to
visually inspect the files afterward to ensure the changes took place.

###### Initialize PostgreSQL (if needed):

```bash
$> initdb -D /path/to/data/directory
```

###### Create Target Database (if needed):

```bash
$> createdb <database>
```

####### Install ```set_user``` functions:

Edit postgresql.conf and add ```set_user``` to the shared_preload_libraries
line, optionally also setting block_alter_system and/or block_copy_program.

First edit postgresql.conf in your favorite editor:

```
$> vi $PGDATA/postgresql.conf
```

Then add these lines to the end of the file:
```
shared_preload_libraries = 'set_user'
set_user.block_alter_system = on
set_user.block_copy_program = on
set_user.block_log_statement = on
```

Finally, restart PostgreSQL (method may vary):

```
#> service postgresql restart
```

Install the extension into your database:

```bash
psql <database>
CREATE EXTENSION set_user;
```


## GUC Parameters

##### (currently none used)

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

#################################
# OS command line, terminal 2
#################################
psql -U dba_user <dbname>

---------------------------------
-- psql command line, terminal 2
---------------------------------
SELECT set_user('postgres');
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
STATEMENT:  SELECT set_user('postgres');
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

##  Contacts

Crunchy SELinux PGSQL is provided as a component of Crunchy Certified PostgreSQL.
Crunchy Data provides supported, packaged versions of Crunchy SELinux PGSQL and
Crunchy Certified PostgreSQL for commercial users.  For more information, please
contact info@crunchydata.com
