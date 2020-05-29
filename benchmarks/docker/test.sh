#!/bin/bash

./docker_build.sh && docker run -it -v "/var/run/docker.sock:/var/run/docker.sock:rw" -v "/tmp/:/tmp/:rw" --network=host --env NH_NH_DOCKER_IMAGE="envoyproxy/nighthawk-dev:latest" --env ENVOY_DOCKER_IMAGE_TO_TEST="envoyproxy/envoy-dev:f61b096f6a2dd3a9c74b9a9369a6ea398dbe1f0f" --env ENVOY_PATH="envoy" --env ENVOY_IP_TEST_VERSIONS=v4only oschaaf/benchmark-dev:latest python3 ../../benchmarks --log-cli-level=debug -vvvv
