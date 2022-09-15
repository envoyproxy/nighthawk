#!/bin/bash

set -eo pipefail
set +x
set -u

BAZEL="bazel"

if [[ "$OSTYPE" == "darwin"* ]]; then
    # On OSX we update the docker env vars to the latest
    eval "$(docker-machine env default)"
    # We also update the output location bazel uses, to make sure
    # that we will be able to map paths.
    # TODO(oschaaf): does this work on Linux?:
    export TEST_SERVER_EXTERNAL_IP="$(docker-machine ip)"
fi

pushd $("${BAZEL}" info workspace)
"${BAZEL}" build `bazel query "kind('py_binary', '//benchmarks/...')"`

export ENVOY_IP_TEST_VERSIONS=v4only
export ENVOY_PATH="envoy"
export TMPDIR="$(pwd)/benchmarks/tmp"
export NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest"
export ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:latest"

# run all tests
bazel-bin/benchmarks/benchmarks --log-cli-level=info -vvvv benchmarks/test/
bazel-bin/benchmarks/dynamic_benchmarks --log-cli-level=info -vvvv benchmarks/dynamic_test/

