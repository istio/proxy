#!/usr/bin/env bash

set -e -o pipefail


BUILDOZER="$(realpath "$BUILDOZER")"
JQ_BIN="$(realpath "$JQ_BIN")"
TARGETS="${TARGETS:-}"

if [[ -z "$TARGETS" ]]; then
    echo "TARGETS not set" >&2
    exit 1
fi

cd "$REPO_PATH"

declare -A POOLS

while read -r line; do
    pool=
    name=$($BUILDOZER 'print name' "$line" 2>/dev/null || :)
    if [[ -n $name ]]; then
        props=$($BUILDOZER 'print exec_properties' "$line" 2>/dev/null || :)
        if [[ -n $props && $props != "(missing)" ]]; then
            pool="$(echo "$props" | tr -d '\n' | sed 's/,}/ }/' | "$JQ_BIN" -r '.Pool' || :)"
        fi
    fi
    if [[ -z $pool ]]; then
        continue
    fi
    if [[ -v POOLS["$pool"] ]]; then
        POOLS["$pool"]+="\n$line"
    else
        POOLS["$pool"]="$line"
    fi
done <<< "$(cat "${TARGETS}")"

idx_pool=0
pools=("${!POOLS[@]}")
pool_count="${#pools[@]}"
{
  echo '{'
  for pool in "${!POOLS[@]}"; do
      echo "\"$pool\": ["
      read -ra targets <<< "$(echo -e "${POOLS["${pool}"]}" | tr '\n' ' ')"
      target_count="${#targets[@]}"
      idx_target=0
      for target in "${targets[@]}"; do
          echo -n "\"${target}\""
          if (( idx_target < target_count - 1 )); then
              echo ','
          else
              echo
          fi
          (( idx_target++ )) || :
      done
      echo -n ']'
      if (( idx_pool < pool_count - 1 )); then
          echo ','
      else
          echo
      fi
      (( idx_pool++ )) || :
  done
  echo '}'
}
