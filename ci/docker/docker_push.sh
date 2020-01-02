#!/bin/bash

# Do not ever set -x here, it is a security hazard as it will place the credentials below in the
# CircleCI logs.
set -e
set +x

if [[ -n "$CIRCLE_PULL_REQUEST" ]]; then
    echo 'Ignoring PR branch for docker push.'
    exit 0
fi

DOCKER_IMAGE_PREFIX="${DOCKER_IMAGE_PREFIX:-envoyproxy/nighthawk}"

# push the nighthawk image on tags or merge to master
if [[ -n "$CIRCLE_TAG" ]] || [[  "$CIRCLE_BRANCH" = 'master' ]]; then
    docker login -u "$DOCKERHUB_USERNAME" -p "$DOCKERHUB_PASSWORD"
    docker push "${DOCKER_IMAGE_PREFIX}-dev:latest"
    docker tag "${DOCKER_IMAGE_PREFIX}-dev:latest" "${DOCKER_IMAGE_PREFIX}-dev:${CIRCLE_SHA1}"
    docker push "${DOCKER_IMAGE_PREFIX}-dev:${CIRCLE_SHA1}"
else
    echo 'Ignoring non-master branch for docker push.'
fi
