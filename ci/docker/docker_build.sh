#!/bin/bash

# Builds a docker image nighthawk-dev:latest containing the stripped binaries
# based on a pre-build bazel-bin directory (with "-c opt" set).

# Stop on errors.
set -e

# NOTE: explicit no -x for verbose commands. Because this is run in CI, doing so may result in
# publishing sensitive information into public CI logs if someone makes a change in a
# consuming script that is off guard.

DOCKER_IMAGE_PREFIX="${DOCKER_IMAGE_PREFIX:-envoyproxy/nighthawk}"

BINARIES=(nighthawk_test_server nighthawk_client nighthawk_service nighthawk_output_transform nighthawk_adaptive_load_client)
BAZEL_BIN="$(bazel info -c opt bazel-bin)"
WORKSPACE="$(bazel info workspace)"
TMP_DIR="${WORKSPACE}/tmp-docker-build-context"

rm -rf "${TMP_DIR}"

echo "Preparing docker build context in ${TMP_DIR}"
cp -r "${WORKSPACE}/ci/docker/" "${TMP_DIR}/"

for BINARY in "${BINARIES[@]}"; do
    echo "Copy ${BINARY}"
    TARGET="${TMP_DIR}/${BINARY}"
    # Docker won't follow symlinks
    cp "${BAZEL_BIN}/${BINARY}.stripped" "${TARGET}"
    chmod +w "${TARGET}"
done

cd "${TMP_DIR}"
echo "running docker build ... "
docker build -f "${TMP_DIR}/Dockerfile-nighthawk" -t "${DOCKER_IMAGE_PREFIX}-dev:latest" .
rm -rf "${TMP_DIR}"
echo "docker build finished"
