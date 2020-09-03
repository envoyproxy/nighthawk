#!/bin/bash

# Builds a docker image benchmark-dev:latest containing the benchmark scripts

# Stop on errors.
set -e

# NOTE: explicit no -x for verbose commands. Because this is run in CI, doing so may result in
# publishing sensitive information into public CI logs if someone makes a change in a
# consuming script that is off guard.

DOCKER_NAME="nighthawk-benchmark"
DOCKER_IMAGE_PREFIX="envoyproxy/${DOCKER_NAME}"
WORKSPACE="$(bazel info workspace)"
BAZEL_BIN="$(bazel info -c opt bazel-bin)"
TMP_DIR="${WORKSPACE}/tmp-docker-build-context"

[ -d "${TMP_DIR}"  ] && rm -rf "${TMP_DIR}"

echo "Preparing docker build context in ${TMP_DIR}"
cp -r "${WORKSPACE}/ci/docker/" "${TMP_DIR}/"
cp -rL "${BAZEL_BIN}/benchmarks" "${TMP_DIR}"

cd "${TMP_DIR}"
echo "running docker build ... "
docker build -f "${TMP_DIR}/Dockerfile-${DOCKER_NAME}" -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .
rm -rf "${TMP_DIR}"
echo "docker build finished"
