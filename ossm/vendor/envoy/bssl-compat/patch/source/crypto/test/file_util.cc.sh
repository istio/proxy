#!/bin/bash

set -euo pipefail

uncomment.sh "$1" --comment -h \
  --uncomment-func-impl SkipTempFileTests \
  --uncomment-func-impl GetTempDir \
  --uncomment-regex-range  'TemporaryFile::~TemporaryFile.*' '#endif\n}'
