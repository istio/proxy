#!/usr/bin/env bash

# Start a docker container running a registry locally.
if [ "$( docker container inspect -f '{{.State.Running}}' registry 2> /dev/null )" != "true" ]; then
    docker rm registry >/dev/null 2>&1 || :
    docker run -d -p 6000:6000 -e REGISTRY_HTTP_ADDR=:6000 --name registry registry:2.8.3
fi

readonly IP=$(docker inspect -f '{{range.NetworkSettings.Networks}}{{.IPAddress}}{{end}}' registry)
readonly REF=$(docker run -v ./$3:/$3 --rm gcr.io/go-containerregistry/crane push /$3 $IP:6000/image)

docker pull "${REF/"$IP"/localhost}"