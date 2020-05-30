#!/bin/bash

set -eo pipefail
set +x
set -u

function cleanup() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        docker rm -f $(docker ps -a -q) || true
    fi
}

trap cleanup EXIT

BAZEL="bazel"

if [[ "$OSTYPE" == "darwin"* ]]; then
    # On OSX we update the docker env vars to the latest
    eval "$(docker-machine env default)"
    # We also update the output location bazel uses, to make sure
    # that we will be able to map paths.
    # TODO(oschaaf): does this work on Linux?:
    export TEST_SERVER_EXTERNAL_IP="$(docker-machine ip)"
fi

pushd $($BAZEL info workspace)
$BAZEL build //benchmarks:benchmarks

export ENVOY_IP_TEST_VERSIONS=v4only
export ENVOY_PATH="envoy"
export TMPDIR="$(pwd)/benchmarks/tmp"
export NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest"
export ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:f61b096f6a2dd3a9c74b9a9369a6ea398dbe1f0f"

# run all tests starting with test_http_h1_small
bazel-bin/benchmarks/benchmarks --log-cli-level=info -vvvv -k test_http_h1_small benchmarks/
