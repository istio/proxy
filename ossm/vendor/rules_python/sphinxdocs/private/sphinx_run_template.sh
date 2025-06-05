#!/bin/bash

declare -a args
%SETUP_ARGS%

declare -a sphinx_env
%SETUP_ENV%

for path in "%SOURCE_DIR_RUNFILES_PATH%" "%SOURCE_DIR_EXEC_PATH%"; do
  if [[ -e $path ]]; then
    source_dir=$path
    break
  fi
done

if [[ -z "$source_dir" ]]; then
    echo "Could not find source dir"
    exit 1
fi

for path in "%SPHINX_RUNFILES_PATH%" "%SPHINX_EXEC_PATH%"; do
  if [[ -e $path ]]; then
    sphinx=$path
    break
  fi
done

if [[ -z $sphinx ]]; then
  echo "Could not find sphinx"
  exit 1
fi

output_dir=${SPHINX_OUT:-/tmp/sphinx-out}

set -x
exec env "${sphinx_env[@]}" -- "$sphinx" "${args[@]}" "$@" "$source_dir" "$output_dir"
