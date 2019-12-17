#!/bin/bash

set -ex

for BINARY in nighthawk_test_server nighthawk_client nighthawk_service nighthawk_output_transform; do
    DOCKER_NAME=$(echo ${BINARY} | tr _ -)
    DOCKER_IMAGE_PREFIX="envoyproxy/${DOCKER_NAME}"

    # Docker won't follow symlinks
    cp bazel-bin/${BINARY} .

    docker build -f ci/Dockerfile-${DOCKER_NAME} -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .

    rm -f ${BINARY}
done
