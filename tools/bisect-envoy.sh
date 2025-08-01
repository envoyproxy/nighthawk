#!/bin/bash

# Utility file for running `git bisect` to find the Envoy commit that breaks
# the Nighthawk build or test.

set -e

NIGHTHAWK_DIR=$1
if [ -z "$NIGHTHAWK_DIR" ]; then
    echo "Error: Nighthawk directory not provided as the first argument."
    exit 1
fi

# This script is executed from within the Envoy repository checkout.
# The commit to test must already be checked out (e.g. by 'git bisect').
ENVOY_COMMIT=$(git rev-parse HEAD)

ARCHIVE_URL="https://github.com/envoyproxy/envoy/archive/${ENVOY_COMMIT}.tar.gz"
curl -sL -o ./envoy.tar.gz "${ARCHIVE_URL}"
ENVOY_SHA=$(shasum -a 256 ./envoy.tar.gz | cut -f1 -d" ")
rm ./envoy.tar.gz

# Update the Envoy commit and SHA in Nighthawk's repository definitions.
sed -i -e "s/ENVOY_COMMIT = \".*\"/ENVOY_COMMIT = \"${ENVOY_COMMIT}\"/" \
  "${NIGHTHAWK_DIR}/bazel/repositories.bzl"
sed -i -e "s/ENVOY_SHA = \".*\"/ENVOY_SHA = \"${ENVOY_SHA}\"/" \
  "${NIGHTHAWK_DIR}/bazel/repositories.bzl"

# Change to the Nighthawk directory and run the tests.
cd "${NIGHTHAWK_DIR}"
./ci/do_ci.sh test
