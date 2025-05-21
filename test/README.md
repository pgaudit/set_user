# Testing

Testing is performed using a Docker container. First build the container with the desired PostgreSQL version:
```
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) --build-arg PGVER=17 -f test/Dockerfile.debian -t set_user-test .
```
Then run the test:
```
docker run --rm -v $(pwd):/set_user set_user-test /set_user/test/test.sh
```
