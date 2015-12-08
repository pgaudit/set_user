# PostgreSQL set_user Extension Module

## Syntax

```
set_user(text rolename) returns text
reset_user() returns text
```

## Inputs

```rolename``` is the role to be transitioned to.

## Description

  This PostgreSQL extension allows privilege escalation with enhanced logging and
  control. It provides an additional layer of logging and control when unprivileged
  users must escalate themselves to superuser or object owner roles in order to
  perform needed maintenance tasks. Specifically, when an allowed user executes
  ```set_user('rolename')```, several actions occur:

  * The current effective user becomes ```rolename```.
  * The role transition is logged, with specific notation if ```rolename``` is a superuser.
  * log_statement setting is set to "all", meaning every SQL statement executed while in this state will also get logged.

  When finished with required actions as ```rolename```, the ```reset_user()``` function
  is executed to restore the original user. At that point, these actions occur:

  * Role transition is logged.
  * log_statement setting is set to its original value.

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

## Caveats

  In its current state, this extension cannot prevent ```rolename``` from manually
  changing the log_statement setting to "none". However, the act of changing the
  setting would itself be logged.

## TODO

  The following changes/enhancements are contemplated:

  * Block changes to the log_statement setting while set_user() is active.
  * Block ALTER SYSTEM commands while set_user() is active.

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

####### Install ```crunchy-selinux-pgsql``` functions:

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
-- verify there are no superusers that can login directly
SELECT rolname FROM pg_authid WHERE rolsuper and rolcanlogin;
 rolname
---------
(0 rows)
```

##  Licensing

  Please see the [LICENSE](./LICENSE) file.

##  Contacts
  Crunchy SELinux PGSQL is provided as a component of Crunchy Certified PostgreSQL.
  Crunchy Data provides supported, packaged versions of Crunchy SELinux PGSQL and
  Crunchy Certified PostgreSQL for commercial users.  For more information, please
  contact info@crunchydata.com
