#!/bin/bash

set -eo pipefail
set +x
set -u

# The benchmark logs and artifacts will be dropped here
OUTDIR="/home/oschaaf/code/envoy-perf-vscode/nighthawk/benchmarks/tmp/"
# Used to map the test that we want to see executed into the docker container
TEST_DIR="/home/oschaaf/code/envoy-perf-vscode/nighthawk/benchmarks/test/"

# Rebuild the docker in case something changed.
./docker_build.sh && 
docker run -it --rm \
  -v "/var/run/docker.sock:/var/run/docker.sock:rw" \
  -v "${OUTDIR}:${OUTDIR}:rw" \
  -v "${TEST_DIR}:/usr/local/bin/benchmarks/benchmarks.runfiles/nighthawk/benchmarks/external_tests/" \
  --network=host \
  --env NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest" \
  --env ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:f61b096f6a2dd3a9c74b9a9369a6ea398dbe1f0f" \
  --env TMPDIR="${OUTDIR}" \
  oschaaf/benchmark-dev:latest ./benchmarks --log-cli-level=info -vvvv 
