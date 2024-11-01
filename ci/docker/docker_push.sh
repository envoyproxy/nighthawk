#!/bin/bash

# Do not ever set -x here, it might leak credentials.
set -e
set +x

# GitHub Actions uses full refs like "refs/heads/main"
MAIN_BRANCH="refs/heads/main"

DOCKER_IMAGE_PREFIX="${DOCKER_IMAGE_PREFIX:-envoyproxy/nighthawk}"

echo "Running docker_push.sh for DOCKER_IMAGE_PREFIX=${DOCKER_IMAGE_PREFIX}, BRANCH=${GH_BRANCH} and SHA1=${GH_SHA1}."

# Only push images for main builds.
if [[ "${GH_BRANCH}" != "${MAIN_BRANCH}" ]]; then
  echo 'Ignoring non-main branch or tag for docker push.'
  exit 0
fi

if [[ -z "${DOCKERHUB_USERNAME}" || -z "${DOCKERHUB_PASSWORD}" ]]; then
    echo "DOCKERHUB_ credentials not set, unable to push" >&2
    exit 1
fi

docker login -u "$DOCKERHUB_USERNAME" -p "$DOCKERHUB_PASSWORD"

docker push "${DOCKER_IMAGE_PREFIX}-dev:latest"
docker tag "${DOCKER_IMAGE_PREFIX}-dev:latest" \
  "${DOCKER_IMAGE_PREFIX}-dev:${GH_SHA1}"
