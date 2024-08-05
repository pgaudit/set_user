# Testing

Testing is performed using a Docker container. First build the container:
```
podman build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -f test/Dockerfile.debian -t set_user-test .
```
or
```
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -f test/Dockerfile.rhel -t set_user-test .
```
Then run the test:
```
docker run --rm -v $(pwd):/set_user set_user-test /set_user/test/test.sh
```

HOST: docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -f test/Dockerfile.debian -t set_user-test .
HOST: docker run --rm -v $(pwd):/set_user set_user-test /set_user/test/test.sh
HOST: podman run -it --rm -v $(pwd):/set_user set_user-test bash

CHECKOUT: PRIOR VERSION

DOCKER: make -C /set_user clean all USE_PGXS=1
DOCKER: sudo bash -c "PATH=${PATH?} make -C /set_user install USE_PGXS=1"
DOCKER: ${PGBIN}/pg_ctl -w start -D ${PGDATA}
DOCKER: psql -c 'create extension set_user'
DOCKER: psql -c 'select * from pg_extension'
DOCKER: ${PGBIN}/pg_ctl -w stop -D ${PGDATA}

CHECKOUT: NEW VERSION

DOCKER: make -C /set_user clean all USE_PGXS=1
DOCKER: sudo bash -c "PATH=${PATH?} make -C /set_user install USE_PGXS=1"
DOCKER: ${PGBIN}/pg_ctl -w start -D ${PGDATA}
DOCKER: psql -c "alter extension set_user update to '1.2.4'"
DOCKER: psql -c 'select * from pg_extension'

DOCKER: exit
