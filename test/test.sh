#!/bin/bash
set -e

# Clean and build set_user
make -C /set_user clean all USE_PGXS=1

# Install set_user so postgres will start with shared_preload_libraries set
sudo bash -c "PATH=${PATH?} make -C /set_user install USE_PGXS=1"

# Start postgres
${PGBIN}/pg_ctl -w start -D ${PGDATA}

# Test set_user
make -C /set_user installcheck USE_PGXS=1
