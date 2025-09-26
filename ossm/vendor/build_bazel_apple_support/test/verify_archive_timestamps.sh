#!/bin/bash

set -euo pipefail
set -x

readonly binary="%{binary}s"

if [[ "$binary" != *_lipo.a ]]; then
  echo "$binary: error: binary name does not end in _lipo.a" >&2
  exit 1
fi

ar -tv "$binary"

# Based on timezones, ar -tv may show the timestamp of the contents as either
# Dec 31 1969 or Jan 1 1970 -- either is fine.
# We would use 'date' here, but the format is slightly different (Jan 1 vs.
# Jan 01).
if ! ar -tv "$binary" | grep "objc_lib" | grep "Dec 31" | grep "1969" \
  && ! ar -tv "$binary" | grep "objc_lib" | grep "Jan  1" | grep "1970"
then
  echo "error: timestamp of contents of archive file should be zero" >&2
  exit 1
fi
