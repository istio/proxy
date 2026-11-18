#!/usr/bin/env bash

# Requirements 
#   - crane
#   - jq
#   - awk
#  
#  ./examples/credential_helper/auth.sh <<< '{"uri":"https://public.ecr.aws/token/?scope\u003drepository:lambda/python:pull\u0026service\u003dpublic.ecr.aws"}'
#  ./examples/credential_helper/auth.sh <<< '{"uri":"https://public.ecr.aws/v2/lambda/python/manifests/3.11.2024.01.25.10"}'

function log () {
    echo $@ >> "/tmp/oci_auth.log"
}

log ""
log "Authenticating"

input=$(cat)
log "Payload: $input"

uri=$(jq -r ".uri" <<< $input)
log "URI: $uri"

host="$(awk -F[/:] '{print $4}' <<< $uri)"
log "Host: $host"


if [[ $input == *"/token"* ]]; then
    log "Auth: None"
    echo "{}"
    exit 1
fi

repository=$(awk -F'^https?://|v2/|/manifests|/blobs' '{print $2 $3}' <<< "$uri")
log "Repository: $repository"


ACCEPTED_MEDIA_TYPES='[
    "application/vnd.docker.distribution.manifest.v2+json",
    "application/vnd.docker.distribution.manifest.list.v2+json",
    "application/vnd.oci.image.manifest.v1+json",
    "application/vnd.oci.image.index.v1+json"
]'


# This will write the response to stdout in a format that Bazels credential helper protocol understands.
# Since this is called by Bazel, users won't bee seeing output of this.
crane auth token "$repository" | 
jq --argjson accept "$ACCEPTED_MEDIA_TYPES" \
'{headers: {Authorization: [("Bearer " + .token)], Accept: [($accept | join(", "))], "Docker-Distribution-API-Version": ["registry/2.0"] }}'

if [[ $? != 0 ]]; then 
    log "Auth: Failed"
    exit 1
fi 
log "Auth: Complete"


# Alternatively you can call an external program such as `docker-credential-ecr-login` to perform the token exchange.
