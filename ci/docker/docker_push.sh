set -e
set +x

docker login -u "$DOCKERHUB_USERNAME" -p "$DOCKERHUB_PASSWORD"
SOURCE_IMAGE="envoyproxy/nighthawk-dev:latest"
PUSH_IMAGE="envoyproxy/nighthawk-dev:latest"
docker tag "${SOURCE_IMAGE}" "${PUSH_IMAGE}"
docker push "${PUSH_IMAGE}"
