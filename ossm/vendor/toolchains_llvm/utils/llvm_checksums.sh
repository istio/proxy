#!/bin/bash
# Copyright 2022 The Bazel Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

use_github_host=0
tmp_dir=
download=1

while getopts "t:v:ghD" opt; do
  case "${opt}" in
  "t") tmp_dir="${OPTARG}" ;;
  "v") llvm_version="${OPTARG}" ;;
  "g") use_github_host=1 ;;
  "h")
    echo "Usage:"
    echo "-t <tempdir> - Optional: Specify a temp directory to download distributions to."
    echo "-v <version> - Version of clang+llvm to use."
    echo "-g           - Use github to download releases."
    exit 2
    ;;
  "D") download=0 ;;
  *)
    echo "invalid option: -${OPTARG}"
    exit 1
    ;;
  esac
done

if [[ -z ${llvm_version-} ]]; then
  echo "Usage: ${BASH_SOURCE[0]} [-t <tempdir>] [-g] -v <llvm_version>"
  exit 1
fi

cleanup() {
  rc=$?
  rm -rf "${tmp_dir}"
  exit "${rc}"
}

if [[ -z "${tmp_dir}" ]]; then
  tmp_dir="$(mktemp -d)"
  echo "Using temp dir: '${tmp_dir}'"
  trap 'cleanup' INT HUP QUIT TERM EXIT
elif [[ ! -r "${tmp_dir}" ]]; then
  echo "Temp directory does not exist: '${tmp_dir}'."
  exit 2
fi

llvm_host() {
  local url_base="releases.llvm.org/${llvm_version}"
  output_dir="${tmp_dir}/${url_base}"
  wget --recursive --level 1 --directory-prefix="${tmp_dir}" \
    --accept-regex "(clang%2bllvm|LLVM)-.*tar.(xz|gz)$" "http://${url_base}/"
}

github_host() {
  output_dir="${tmp_dir}/${llvm_version}"
  mkdir -p "${output_dir}"
  if ((download)); then
    echo ""
    echo "===="
    echo "Checksums for clang+llvm distributions are (${output_dir}):"
    echo "    # ${llvm_version}"
    curl -s "https://api.github.com/repos/llvm/llvm-project/releases/tags/llvmorg-${llvm_version}" |
      tee ./releases.json |
      jq -r '.assets[]|select(any(.name; test("^(clang[+]llvm|LLVM)-.*tar.(xz|gz)$")))|"    \""+(.browser_download_url|split("/")|.[-1]|sub("%2B";"+"))+"\": \""+.digest+"\","'
    exit 0
  fi
  (
    cd "${output_dir}"
    curl -s "https://api.github.com/repos/llvm/llvm-project/releases/tags/llvmorg-${llvm_version}" |
      tee ./releases.json |
      jq '.assets[]|select(any(.name .digest; test("^(clang[+]llvm|LLVM)-.*tar.(xz|gz)$")))|.browser_download_url' |
      tee ./filtered_urls.txt |
      xargs -n1 curl -L -O -C -
  )
}

if ((use_github_host)); then
  github_host
else
  llvm_host
fi

echo ""
echo "===="
echo "Checksums for clang+llvm distributions are (${output_dir}):"
echo "    # ${llvm_version}"
find "${output_dir}" -type f \( -name 'clang%2?llvm-*.tar.*' -o -name 'LLVM-*.tar.*' \) \( -name '*.gz' -o -name '*.xz' \) -exec shasum -a 256 {} \; |
  sed -e "s@${output_dir}/@@" |
  awk '{ printf "    \"%s\": \"%s\",\n", $2, $1 }' |
  sed -e 's/%2[Bb]/+/' |
  sort
