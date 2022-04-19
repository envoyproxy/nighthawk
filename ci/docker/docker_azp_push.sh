#!/bin/bash

# Do not ever set -x here, it might leak credentials.
set -e
set +x

# This is how AZP identifies the branch, see the Build.SourceBranch variable in:
# https://docs.microsoft.com/en-us/azure/devops/pipelines/build/variables?view=azure-devops&tabs=yaml#build-variables-devops-services
MAIN_BRANCH="refs/heads/main"

DOCKER_IMAGE_PREFIX="${DOCKER_IMAGE_PREFIX:-envoyproxy/nighthawk}"

# Only push images for main builds.
#if [[ "${AZP_BRANCH}" != "${MAIN_BRANCH}" ]]; then
#  echo 'Ignoring non-main branch or tag for docker push.'
#  exit 0
#fi

docker login -u "$DOCKERHUB_USERNAME" -p "$DOCKERHUB_PASSWORD"

echo docker push "${DOCKER_IMAGE_PREFIX}-dev:latest"
echo docker tag "${DOCKER_IMAGE_PREFIX}-dev:latest" \
  "${DOCKER_IMAGE_PREFIX}-dev:${AZP_SHA1}"
