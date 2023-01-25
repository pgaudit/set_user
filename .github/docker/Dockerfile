# set_user Docker image
# This image is used for testing the set_user build process
ARG PGVER
FROM postgres:${PGVER}
ARG PGVER
ARG DEVPKG
ENV DEBIAN_FRONTEND=noninteractive

COPY . /src/set_user
WORKDIR /src/set_user
RUN apt-get update && \
	apt-get -y upgrade && \
	apt-get -y install postgresql-server-dev-${DEVPKG} make gcc
RUN make install
