#!/bin/bash

set -ex

BINARIES=(nighthawk_test_server nighthawk_client nighthawk_service nighthawk_output_transform)

DOCKER_NAME="nighthawk"
DOCKER_IMAGE_PREFIX="envoyproxy/${DOCKER_NAME}"

for BINARY in "${BINARIES[@]}"; do
    # Docker won't follow symlinks
    cp bazel-bin/${BINARY} .
done

docker build -f ci/Dockerfile-${DOCKER_NAME} -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .

for BINARY in "${BINARIES[@]}"; do
    rm -f ${BINARY}
done
