FROM debian:stretch
LABEL maintainer="Lokal Developers <dev@lokal.org>"
LABEL description="Dockerised LokalCore, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
