# Testing

Testing is performed using a Docker container. First build the container:
```
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -f test/Dockerfile.debian -t set_user-test .
```
or
```
docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) -f test/Dockerfile.rhel -t set_user-test .
```
Then run the test:
```
docker run --rm -v $(pwd):/set_user set_user-test /set_user/test/test.sh
```
