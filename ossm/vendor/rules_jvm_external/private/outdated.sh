#!/usr/bin/env bash

set -e -o pipefail

outdated_jar_path=$1
artifacts_file_path=$2
boms_file_path=$3
repositories_file_path=$4
extra_option_flag=$5

java {proxy_opts} -jar "$outdated_jar_path" \
  --artifacts-file "$artifacts_file_path" \
  --boms-file "$boms_file_path" \
  --repositories-file "$repositories_file_path" \
  $extra_option_flag
