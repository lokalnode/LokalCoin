#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR/..

DOCKER_IMAGE=${DOCKER_IMAGE:-lokalpay/lokald-develop}
DOCKER_TAG=${DOCKER_TAG:-latest}

BUILD_DIR=${BUILD_DIR:-.}

rm docker/bin/*
mkdir docker/bin
cp $BUILD_DIR/src/lokald docker/bin/
cp $BUILD_DIR/src/lokal-cli docker/bin/
cp $BUILD_DIR/src/lokal-tx docker/bin/
strip docker/bin/lokald
strip docker/bin/lokal-cli
strip docker/bin/lokal-tx

docker build --pull -t $DOCKER_IMAGE:$DOCKER_TAG -f docker/Dockerfile docker
