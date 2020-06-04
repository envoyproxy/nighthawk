#!/bin/bash

# Builds a docker image nighthawk-dev:latest containing the stripped binaries
# based on a pre-build bazel-bin directory (with "-c opt" set).

set -eo pipefail
set +x
set -u

# NOTE: explicit no -x for verbose commands. Because this is run in CI, doing so may result in
# publishing sensitive information into public CI logs if someone makes a change in a
# consuming script that is off guard.

DOCKER_NAME="benchmark"
DOCKER_IMAGE_PREFIX="${USER}/${DOCKER_NAME}"
BAZEL_BIN="$(bazel info bazel-bin)"
WORKSPACE="$(bazel info workspace)"
bazel build //benchmarks:benchmarks
TMP_DIR="$(mktemp -d)"
PUSH=${PUSH:-0}  

echo "Preparing docker build context in ${TMP_DIR}"
# We flatten any symlinks to make this work on Linux (OSX doesn't need this)
cp -Lr "${WORKSPACE}/benchmarks/docker/" "${TMP_DIR}/"
cp -Lr "${BAZEL_BIN}/benchmarks" "${TMP_DIR}/"


cd "${TMP_DIR}"
echo "running docker build ... "
docker build -f "${TMP_DIR}/docker/Dockerfile-${DOCKER_NAME}" -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .
rm -rf "${TMP_DIR}"
echo "docker build finished"

if [[ $PUSH == "1" ]]; then
    echo "pushing ${DOCKER_IMAGE_PREFIX}-dev:latest .."
    docker tag "${DOCKER_IMAGE_PREFIX}-dev:latest" "${DOCKER_IMAGE_PREFIX}-dev:latest"
    docker push "${DOCKER_IMAGE_PREFIX}-dev:latest"
    echo "docker image pushed"
fi