#!/usr/bin/env bash
# Produce an mtree specification file for creating a tarball in the form needed for `docker load`.
# This doesn't actually run `tar` because that large output is often not required by any other actions in the graph and causes load on the cache.
set -o pipefail -o errexit -o nounset

readonly FORMAT="{{format}}"
readonly JQ="{{jq_path}}"
readonly COREUTILS="{{coreutils_path}}"
readonly TAR="{{tar}}"
readonly IMAGE_DIR="{{image_dir}}"
readonly REPOTAGS=($("${COREUTILS}" cat "{{tags}}"))
readonly INDEX_FILE="${IMAGE_DIR}/index.json"

readonly OUTPUT="{{output}}"
# Write tar manifest in mtree format
# https://man.freebsd.org/cgi/man.cgi?mtree(8)
# so that tar produces a deterministic output.
function add_to_tar() {
    content=$1
    tar_path=$2
    echo >>"${OUTPUT}" "${tar_path} uid=0 gid=0 mode=0755 time=1672560000 type=file content=${content}"
}

MANIFEST_DIGEST=$(${JQ} -r '.manifests[0].digest | sub(":"; "/")' "${INDEX_FILE}" | "${COREUTILS}" tr  -d '"')

MANIFESTS_LENGTH=$("${JQ}" -r '.manifests | length' "${INDEX_FILE}")
if [[ "${MANIFESTS_LENGTH}" != 1 ]]; then
  echo >&2 "Expected exactly one manifest in ${INDEX_FILE}"
  exit 1
fi

MEDIA_TYPE=$("${JQ}" -r ".manifests[0].mediaType" "${INDEX_FILE}")

# Check that we know how to generate the output format given the input format.
# We may expand the supported options here in the future, but for now,
if [[ "${FORMAT}" != "docker" && "${FORMAT}" != "oci" ]]; then
  echo >&2 "Unknown format: ${FORMAT}. Only support docker|oci"
  exit 1
fi
if [[ "${FORMAT}" == "oci" && "${MEDIA_TYPE}" != "application/vnd.oci.image.index.v1+json" && "${MEDIA_TYPE}" != "application/vnd.docker.distribution.manifest.v2+json" ]]; then
  echo >&2 "Format oci is only supported for oci_image_index targets but saw ${MEDIA_TYPE}"
  exit 1
fi
if [[ "${FORMAT}" == "docker" && "${MEDIA_TYPE}" != "application/vnd.oci.image.manifest.v1+json" && "${MEDIA_TYPE}" != "application/vnd.docker.distribution.manifest.v2+json" ]]; then
  echo >&2 "Format docker is only supported for oci_image targets but saw ${MEDIA_TYPE}"
  exit 1
fi

if [[ "${FORMAT}" == "oci" ]]; then
  # Handle multi-architecture image indexes.
  # Ideally the toolchains we rely on would output these for us, but they don't seem to.

  add_to_tar "${IMAGE_DIR}/oci-layout" oci-layout

  INDEX_FILE_MANIFEST_DIGEST=$("${JQ}" -r '.manifests[0].digest | sub(":"; "/")' "${INDEX_FILE}" | "${COREUTILS}" tr  -d '"')
  INDEX_FILE_MANIFEST_BLOB_PATH="${IMAGE_DIR}/blobs/${INDEX_FILE_MANIFEST_DIGEST}"

  add_to_tar "${INDEX_FILE_MANIFEST_BLOB_PATH}" "blobs/${INDEX_FILE_MANIFEST_DIGEST}"

  IMAGE_MANIFESTS_DIGESTS=($("${JQ}" -r '.manifests[] | .digest | sub(":"; "/")' "${INDEX_FILE_MANIFEST_BLOB_PATH}"))

  for IMAGE_MANIFEST_DIGEST in "${IMAGE_MANIFESTS_DIGESTS[@]}"; do
    IMAGE_MANIFEST_BLOB_PATH="${IMAGE_DIR}/blobs/${IMAGE_MANIFEST_DIGEST}"
    add_to_tar "${IMAGE_MANIFEST_BLOB_PATH}" "blobs/${IMAGE_MANIFEST_DIGEST}"

    CONFIG_DIGEST=$("${JQ}" -r '.config.digest  | sub(":"; "/")' ${IMAGE_MANIFEST_BLOB_PATH})
    CONFIG_BLOB_PATH="${IMAGE_DIR}/blobs/${CONFIG_DIGEST}"
    add_to_tar "${CONFIG_BLOB_PATH}" "blobs/${CONFIG_DIGEST}"

    LAYER_DIGESTS=$("${JQ}" -r '.layers | map(.digest | sub(":"; "/"))' "${IMAGE_MANIFEST_BLOB_PATH}")
    for LAYER_DIGEST in $("${JQ}" -r ".[]" <<< $LAYER_DIGESTS); do
      add_to_tar "${IMAGE_DIR}/blobs/${LAYER_DIGEST}" blobs/${LAYER_DIGEST}
    done
  done


  # Repeat the first manifest entry once per repo tag.
  repotags="${REPOTAGS[@]+"${REPOTAGS[@]}"}"
  "${JQ}" >"{{json_out}}" \
    -r --arg repo_tags "$repotags" \
    '.manifests[0] as $manifest | .manifests = ($repo_tags | split(" ") | map($manifest * {annotations:{"org.opencontainers.image.ref.name":.}}))' "${INDEX_FILE}"
  add_to_tar "{{json_out}}" index.json

  exit 0
fi

MANIFEST_DIGEST=$(${JQ} -r '.manifests[0].digest | sub(":"; "/")' "${IMAGE_DIR}/index.json" | "${COREUTILS}" tr  -d '"')
MANIFEST_BLOB_PATH="${IMAGE_DIR}/blobs/${MANIFEST_DIGEST}"

CONFIG_DIGEST=$(${JQ} -r '.config.digest  | sub(":"; "/")' ${MANIFEST_BLOB_PATH})
CONFIG_BLOB_PATH="${IMAGE_DIR}/blobs/${CONFIG_DIGEST}"

LAYERS=$(${JQ} -cr '.layers | map(.digest | sub(":"; "/"))' ${MANIFEST_BLOB_PATH})

add_to_tar "${CONFIG_BLOB_PATH}" "blobs/${CONFIG_DIGEST}"

for LAYER in $(${JQ} -r ".[]" <<< $LAYERS); do
  add_to_tar "${IMAGE_DIR}/blobs/${LAYER}" "blobs/${LAYER}.tar.gz"
done

repotags="${REPOTAGS[@]+"${REPOTAGS[@]}"}"
"${JQ}" > "{{json_out}}" \
  -n '.[0] = {"Config": $config, "RepoTags": ($repo_tags | split(" ") | map(select(. != ""))), "Layers": $layers | map( "blobs/" + . + ".tar.gz") }' \
  --arg repo_tags "$repotags" \
  --arg config "blobs/${CONFIG_DIGEST}" \
  --argjson layers "${LAYERS}"

add_to_tar "{{json_out}}" "manifest.json"
