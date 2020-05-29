#!/bin/bash

# Builds a docker image nighthawk-dev:latest containing the stripped binaries
# based on a pre-build bazel-bin directory (with "-c opt" set).

# Stop on errors.
set -e

# NOTE: explicit no -x for verbose commands. Because this is run in CI, doing so may result in
# publishing sensitive information into public CI logs if someone makes a change in a
# consuming script that is off guard.

DOCKER_NAME="benchmark"
DOCKER_IMAGE_PREFIX="oschaaf/${DOCKER_NAME}"
BAZEL_BIN="$(bazel info bazel-bin)"
WORKSPACE="$(bazel info workspace)"
bazel build //benchmarks:benchmarks
TMP_DIR="${WORKSPACE}/tmp-docker-build-context"

rm -rf "${TMP_DIR}"

echo "Preparing docker build context in ${TMP_DIR}"
cp -r "${WORKSPACE}/benchmarks/docker/" "${TMP_DIR}/"
cp -r "${BAZEL_BIN}/benchmarks" "${TMP_DIR}/"


cd "${TMP_DIR}"
echo "running docker build ... "
docker build -f "${TMP_DIR}/Dockerfile-${DOCKER_NAME}" -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .
rm -rf "${TMP_DIR}"
echo "docker build finished"