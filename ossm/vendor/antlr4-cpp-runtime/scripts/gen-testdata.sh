#!/bin/bash

# Copyright Istio Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -u
set -e

function usage() {
  echo "$0
    -c control whether to check generated file having diff or not."
  exit 1
}

CHECK=0

while getopts c arg ; do
  case "${arg}" in
    c) CHECK=1;;
    *) usage;;
  esac
done

OUT_DIR=$(mktemp -d -t testdata.XXXXXXXXXX) || { echo "Failed to create temp file"; exit 1; }

SCRIPTPATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOTDIR="${SCRIPTPATH}/.."

mkdir -p "${OUT_DIR}"
cp -R "${ROOTDIR}/testdata/bootstrap" "${OUT_DIR}"
cp -R "${ROOTDIR}/testdata/listener" "${OUT_DIR}"

cd "${OUT_DIR}" || exit
go-bindata --nocompress --nometadata --pkg testdata -o "${ROOTDIR}/testdata/testdata.gen.go" ./...

if [[ "${CHECK}" == "1" ]]; then
    pushd "$ROOTDIR" || exit
    CHANGED=$(git diff-index --name-only HEAD --)
    popd || exit
    if [[ -z "${CHANGED}" ]]; then
        echo "generated test config is not up to date, run 'make gen' to update."
        exit 1
    fi
fi

rm -Rf "${OUT_DIR}"
