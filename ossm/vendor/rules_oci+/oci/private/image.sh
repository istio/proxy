#!/usr/bin/env bash
set -o pipefail -o errexit -o nounset

# Replace PATH with hermetically built jq, regctl, coreutils.
# shellcheck disable=SC2123
PATH="{{jq_path}}"
PATH="{{regctl_path}}:$PATH"
PATH="{{coreutils_path}}:$PATH"

# Constants
readonly USE_TREEARTIFACT_SYMLINKS="{{treeartifact_symlinks}}"
readonly OUTPUT="{{output}}"
readonly REF="ocidir://$OUTPUT:intermediate"
# shellcheck disable=SC2016
readonly ENV_EXPAND_FILTER='[$raw | match("\\${?([a-zA-Z0-9_]+)}?"; "gm")] | reduce .[] as $match (
    {parts: [], prev: 0};
    {parts: (.parts + [$raw[.prev:$match.offset], ($envs[] | select(.key == $match.captures[0].string)).value ]), prev: ($match.offset + $match.length)}
) | .parts + [$raw[.prev:]] | join("")'

function base_from_scratch() {
  local platform="$1"
  # Create a new manifest
  jq -n '{
    schemaVersion: 2,
    mediaType: "application/vnd.oci.image.manifest.v1+json",
    config: { mediaType: "application/vnd.oci.image.config.v1+json", size: 0 },
    layers: []
  }' | update_manifest
  # Create the image config when there is annotations
  jq -n --argjson platform "$platform" '{created: "1970-01-01T00:00:00Z", config:{}, history:[], rootfs:{type: "layers", diff_ids:[]}} + $platform' | update_config >/dev/null
}

function base_from() {
  local path="$1"
  # shellcheck disable=SC2045
  for blob in $(coreutils ls -1 -d "$path/blobs/"*/*); do
    local relative_to_blobs="${blob#"$path/blobs"}"
    coreutils mkdir -p "$OUTPUT/blobs/$(coreutils dirname "$relative_to_blobs")"
    if [[ "$USE_TREEARTIFACT_SYMLINKS" == "1" ]]; then
      # Relative path from `output/blobs/sha256/` to `$blob`
      relative="$(coreutils realpath --relative-to="$OUTPUT/blobs/sha256" "$blob" --no-symlinks)"
      coreutils ln -s "$relative" "$OUTPUT/blobs/$relative_to_blobs"
    else
      coreutils cp --no-preserve=mode "$blob" "$OUTPUT/blobs/$relative_to_blobs"
    fi
  done
  coreutils cp --no-preserve=mode "$path/oci-layout" "$OUTPUT/oci-layout"
  jq '.manifests[0].annotations["org.opencontainers.image.ref.name"] = "intermediate"' "$path/index.json" >"$OUTPUT/index.json"
}

function get_config() {
  regctl blob get "$REF" "$(regctl manifest get "$REF" --format "{{.Config.Digest}}" 2>/dev/null)" 2>/dev/null
}

function update_config() {
  local digest=
  local config=
  config="$(coreutils cat -)"
  digest="$(echo -n "$config" | regctl blob put "$REF" 2>/dev/null)"
  # The embedded .config.data must be kept up to date or dropped. Making the optimal choice is subtle; dropping is always at least correct.
  get_manifest | jq '.config.digest = $digest | .config.size = $size | del(.config.data)' --arg digest "$digest" --argjson size "${#config}" | update_manifest >/dev/null
  echo "$digest"
}

function get_manifest() {
  regctl manifest get "$REF" --format "raw" 2>/dev/null
}

function update_manifest() {
  regctl manifest put "$REF" 2>/dev/null
}

function add_layer() {
  local path="$1"
  local desc=
  local media_type=
  local comp_ext=

  desc="$(coreutils cat "$2")"

  # If the base image uses docker media types, then add new layer with oci-spec
  # interchangable media type.
  if [[ $(get_manifest | jq -r '.mediaType') == "application/vnd.docker."* ]]; then
    media_type="application/vnd.docker.image.rootfs.diff.tar"
    comp_ext="."
  else
    # otherwise, use oci-spec media types.
    media_type="application/vnd.oci.image.layer.v1.tar"
    comp_ext="+"
  fi

  desc="$(jq --arg comp_ext "${comp_ext}" '.compression |= (if . != "" then "\($comp_ext)\(.)" end)' <<< "$desc")"

  new_config_digest=$(
    get_config | jq --argjson desc "$desc" '.rootfs.diff_ids += [$desc.diffid] | .history += [$desc.history]' | update_config
  )

  get_manifest |
    jq '.config.digest = $config_digest |
        .layers += [{size: $desc.size, digest: $desc.digest, mediaType: "\($media_type)\($desc.compression)"}]' \
      --arg config_digest "${new_config_digest}" \
      --argjson desc "${desc}" \
      --arg media_type "${media_type}" | update_manifest

  local digest_path=
  local output_path=
  digest_path="$(jq -r '.digest | sub(":"; "/")' <<< "$desc")"
  output_path="$OUTPUT/blobs/$digest_path"

  if [[ "$USE_TREEARTIFACT_SYMLINKS" == "1" ]]; then
    relative=$(coreutils realpath --no-symlinks --canonicalize-missing --relative-to="$OUTPUT/blobs/sha256" "$path" )
    coreutils ln --force --symbolic "$relative" "$output_path"
  else
    coreutils cp --no-preserve=mode "$path" "$output_path"
  fi
}

CONFIG="{}"

for ARG in "$@"; do
  case "$ARG" in
  --scratch=*)
    base_from_scratch "${ARG#--scratch=}"
    ;;
  --from=*)
    base_from "${ARG#--from=}"
    ;;
  --layer=*)
    IFS='=' read -r layer descriptor <<<"${ARG#--layer=}"
    add_layer "${layer}" "$descriptor"
    ;;
  --env=*)
    # Get environment from existing config
    env=$(get_config | jq '(.config.Env // []) | map(. | split("=") | {"key": .[0], "value": .[1:] | join("=")})')
    while IFS= read -r expansion || [ -n "$expansion" ]; do
      # collect all characters until a `=` is encountered
      key="${expansion%%=*}"
      # skip `length(k) + 1` to collect the rest.
      value="${expansion:${#key}+1}"
      value_from_base=$(jq -nr --arg raw "${value}" --argjson envs "${env}" "${ENV_EXPAND_FILTER}")
      env=$(
        # update the existing env if it exists, or append to the end of env array.
        jq -r --arg key "$key" --arg value "$value_from_base" '. |= (map(.key) | index($key)) as $i | if $i then .[$i]["value"] = $value else . + [{key: $key, value: $value}] end' <<<"$env"
      )
    done <"${ARG#--env=}"

    CONFIG=$(jq --argjson envs "${env}" '.config.Env = ($envs | map("\(.key)=\(.value)"))' <<<"$CONFIG")
    ;;
  --cmd=*)
    CONFIG=$(jq --rawfile cmd "${ARG#--cmd=}" '.config.Cmd = ($cmd | split("\n") | map(select(. | length > 0)) | map(. | sub("%5Cn"; "\n"; "g")))' <<<"$CONFIG")
    ;;
  --entrypoint=*)
    # NOTE: setting entrypoint deletes `.config.Cmd` which is consistent with crane and Dockerfile behavior.
    # See: https://github.com/bazel-contrib/rules_oci/issues/649
    # See: https://github.com/google/go-containerregistry/blob/c3d1dcc932076c15b65b8b9acfff1d47ded2ebf9/cmd/crane/cmd/mutate.go#L107
    CONFIG=$(jq --rawfile entrypoint "${ARG#--entrypoint=}" '.config.Cmd = null | .config.Entrypoint = ($entrypoint | split("\n") | map(select(. | length > 0)) | map(. | sub("%5Cn"; "\n"; "g")))' <<<"$CONFIG")
    ;;
  --exposed-ports=*)
    CONFIG=$(jq --rawfile ep "${ARG#--exposed-ports=}" '.config.ExposedPorts = ($ep | split(",") | map({key: ., value: {}}) | from_entries)' <<<"$CONFIG")
    ;;
  --volumes=*)
    CONFIG=$(jq --rawfile volumes "${ARG#--volumes=}" '.config.Volumes = ($volumes | split(",") | map({key: ., value: {}}) | from_entries)' <<<"$CONFIG")
    ;;
  --user=*)
    CONFIG=$(jq --arg user "${ARG#--user=}" '.config.User = $user' <<<"$CONFIG")
    ;;
  --workdir=*)
    CONFIG=$(jq --arg workdir "${ARG#--workdir=}" '.config.WorkingDir = $workdir' <<<"$CONFIG")
    ;;
  --labels=*)
    CONFIG=$(jq --rawfile labels "${ARG#--labels=}" '.config.Labels += ($labels | split("\n") | map(select(. | length > 0)) | map(. | split("=")) | map({key: .[0], value: .[1:] | join("=")}) | from_entries)' <<<"$CONFIG")
    ;;
  --created=*)
    CONFIG=$(jq --rawfile created "${ARG#--created=}" '.created = $created' <<<"$CONFIG")
    ;;
  --annotations=*)
    get_manifest |
      jq --rawfile annotations "${ARG#--annotations=}" \
      '.annotations += ([($annotations | split("\n") | .[] | select(. != ""))] | map(. | split("=")) | map({key: .[0], value: .[1:] | join("=")}) | from_entries)' |
      update_manifest
    ;;
  *)
    echo "unknown argument ${ARG}"
    exit 1
    ;;
  esac
done

get_config | jq --argjson config "$CONFIG" '. *= $config | del(.config.Cmd|nulls)' | update_config >/dev/null
## TODO: container structure is broken
(JSON="$(coreutils cat "$OUTPUT/index.json")" && jq "del(.manifests[].annotations)" >"$OUTPUT/index.json" <<<"$JSON")
