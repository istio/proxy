#!/bin/bash -e

set -o pipefail

VERSION_FILE="$1"
DEP_DATA="$2"
DEP="$3"
VERSION="$4"

JQ="${JQ_BIN:-jq}"
if [[ -z "$JQ" || ! -x "$JQ" ]]; then
    echo "jq binary not found: ${JQ}" >&2
    exit 1
fi
if [[ -z "$DEP" || -z "$VERSION" ]]; then
    echo "You need to supply dependency and sha to update" >&2
    exit 1
fi
if [[ -n "$VERSION_PATH_REPLACE" ]]; then
    IFS=':' read -ra path_replace <<< "$VERSION_PATH_REPLACE"
    VERSION_FILE="${VERSION_FILE/${path_replace[0]}/${path_replace[1]}/}"
fi
if [[ -n "$VERSION_UPDATE_POST_SCRIPT" ]]; then
    VERSION_UPDATE_POST_SCRIPT="$(realpath "${VERSION_UPDATE_POST_SCRIPT}")"
fi

pushd "${BUILD_WORKSPACE_DIRECTORY}" &> /dev/null
VERSION_FILE="$(realpath "${VERSION_FILE}")"
popd &> /dev/null
REPO_SELECTOR="${REPO_SELECTOR:-".__DEP__.repo"}"
REPO_SELECTOR="${REPO_SELECTOR/__DEP__/${DEP}}"
REPO="$("${JQ}" -r "${REPO_SELECTOR}" "${DEP_DATA}")"
SHA_SELECTOR="${SHA_SELECTOR:-".__DEP__.sha256"}"
SHA_SELECTOR="${SHA_SELECTOR/__DEP__/${DEP}}"
EXISTING_SHA="$("${JQ}" -r "${SHA_SELECTOR}" "${DEP_DATA}")"
VERSION_SELECTOR="${VERSION_SELECTOR:-".__DEP__.version"}"
VERSION_SELECTOR="${VERSION_SELECTOR/__DEP__/${DEP}}"
EXISTING_VERSION="$("${JQ}" -r "${VERSION_SELECTOR}" "${DEP_DATA}")"
URL_SELECTOR="${URL_SELECTOR:-".__DEP__.urls[0]"}"
URL_SELECTOR="${URL_SELECTOR/__DEP__/${DEP}}"
URL="$(${JQ} -r "${URL_SELECTOR}" "${DEP_DATA}")"
DEP_SEARCH="${DEP_SEARCH:-"\"__DEP__\": {"}"
DEP_SEARCH="${DEP_SEARCH/__DEP__/${DEP}}"
VERSION_SEARCH="${VERSION_SEARCH:-"\"version\": \"__EXISTING_VERSION__\","}"
VERSION_SEARCH="${VERSION_SEARCH/__EXISTING_VERSION__/$EXISTING_VERSION}"
SHA_SEARCH="${SHA_SEARCH:-"\"sha256\": \"__EXISTING_SHA__\","}"
SHA_SEARCH="${SHA_SEARCH/__EXISTING_SHA__/$EXISTING_SHA}"


get_sha () {
    local url sha repo version url
    url="$1"
    repo="$2"
    version="$3"
    url="${url//\{repo\}/${repo}}"
    url="${url//\{version\}/${version}}"
    url="${url//${EXISTING_VERSION}/${version}}"
    sha="$(curl -sfL "${url}" | sha256sum | cut -d' ' -f1)" || {
        echo "Failed to fetch asset (${url})" >&2
        exit 1
    }
    printf '%s' "$sha"
}

find_version_line () {
    # This needs to find the correct version to replace
    match="$(\
        grep -n "${DEP_SEARCH}" "${VERSION_FILE}" \
        | cut -d: -f-2)"
    match_ln="$(\
        echo "${match}" \
        | cut -d: -f1)"
    match_ln="$((match_ln + 1))"
    version_match_ln="$(\
        tail -n "+${match_ln}" "${VERSION_FILE}" \
        | grep -n "${VERSION_SEARCH}" \
        | head -n1 \
        | cut -d: -f1)"
    version_match_ln="$((match_ln + version_match_ln - 1))"
    printf '%s' "$version_match_ln"
}

find_sha_line () {
    # This needs to find the correct version to replace
    match="$(\
        grep -n "${DEP_SEARCH}" "${VERSION_FILE}" \
        | cut -d: -f-2)"
    match_ln="$(\
        echo "${match}" \
        | cut -d: -f1)"
    match_ln="$((match_ln + 1))"
    sha_match_ln="$(\
        tail -n "+${match_ln}" "${VERSION_FILE}" \
        | grep -n "${SHA_SEARCH}" \
        | head -n1 \
        | cut -d: -f1)"
    sha_match_ln="$((match_ln + sha_match_ln - 1))"
    printf '%s' "$sha_match_ln"
}

update_sha () {
    local match_ln search replace
    match_ln="$1"
    search="$2"
    replace="$3"
    echo "Updating sha: ${search} -> ${replace}"
    sed -i "${match_ln}s/${search}/${replace}/" "$VERSION_FILE"
}

update_version () {
    local match_ln search replace
    match_ln="$1"
    search="$2"
    replace="$3"
    echo "Updating version: ${search} -> ${replace}"
    sed -i "${match_ln}s/${search}/${replace}/" "$VERSION_FILE"
}

update_dependency () {
    local dep_ln sha
    dep_ln="$(find_version_line)"
    if [[ -z "$dep_ln" ]]; then
        echo "Dependency(${DEP}) not found in ${VERSION_FILE}" >&2
        exit 1
    fi
    sha="$(get_sha "${URL}" "${REPO}" "${VERSION}")"
    if [[ -z "$sha" ]]; then
        echo "Unable to resolve sha for ${DEP}/${VERSION}" >&2
        exit 1
    fi
    sha_ln="$(find_sha_line)"
    if [[ -z "$sha_ln" ]]; then
        echo "Unable to find sha for ${DEP}/${VERSION}" >&2
        exit 1
    fi
    update_version "${dep_ln}" "${EXISTING_VERSION}" "${VERSION}"
    update_sha "${sha_ln}" "${EXISTING_SHA}" "${sha}"
}

update_dependency

if [[ -n "$VERSION_UPDATE_POST_SCRIPT" ]]; then
    # shellcheck disable=SC1090
    . "$VERSION_UPDATE_POST_SCRIPT"
    post_version_update
fi
