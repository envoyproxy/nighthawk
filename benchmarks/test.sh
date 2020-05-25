#!/bin/bash

set -eo pipefail
set +x
set -u

function cleanup() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        #docker-machine stop default
        docker rm -f "$(docker ps -a -q)" || true
        echo "cleanup"
    fi
}

trap cleanup EXIT

BAZEL="bazel"

if [[ "$OSTYPE" == "darwin"* ]]; then
    docker-machine start default || true
    # On OSX we update the docker env vars to the latest
    eval "$(docker-machine env default)"
    # We also update the output location bazel uses, to make sure
    # that we will be able to map paths.

    # sometimes needs 
    #sudo spctl --master-disable
    #bazel info workspace
    #sudo spctl --master-enable

    # TODO(oschaaf): does this work on Linux?:
    export TEST_SERVER_EXTERNAL_IP="$(docker-machine ip)"
fi

pushd $($BAZEL info workspace)
$BAZEL build //benchmarks:benchmarks

export ENVOY_IP_TEST_VERSIONS=v4only
export NH_RUNDIR="$(${BAZEL} info bazel-bin)/"
export NH_CERTDIR="${NH_RUNDIR}benchmarks/benchmarks.runfiles/nighthawk/external/envoy/test/config/integration/certs/"
export NH_CONFDIR="${NH_RUNDIR}benchmarks/benchmarks.runfiles/nighthawk/test/integration/configurations/"
export NH_TEST_SERVER_PATH=nighthawk_test_server
export NH_CLIENT_PATH=nighthawk_client
export NH_OUTPUT_TRANSFORM_PATH=nighthawk_output_transform
export TMPDIR="$(pwd)/benchmarks/tmp"
export NH_NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest"
export ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:74290ef76a76fbbf50f072dc33438791f93f68c7"

# run all tests starting with test_http_h1_small
bazel-bin/benchmarks/benchmarks --log-cli-level=info -vvvv -k test_http_h1_small benchmarks/
