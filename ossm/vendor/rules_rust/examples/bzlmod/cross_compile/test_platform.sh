#!/bin/bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo >&2 "Usage: $0 /path/to/binary file-output"
    exit 1
fi

binary="$1"
want_file_output="$2"

out="$(file -L "${binary}")"

if [[ "${out}" != *"${want_file_output}"* ]]; then
    echo >&2 "Wrong file type: ${out}"
    exit 1
fi