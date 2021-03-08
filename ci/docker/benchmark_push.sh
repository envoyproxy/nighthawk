#!/bin/bash

DOCKER_IMAGE_PREFIX="${DOCKER_IMAGE_PREFIX:-envoyproxy/nighthawk-benchmark}"

source ./ci/docker/docker_push.sh