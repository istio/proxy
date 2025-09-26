# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

command_line="$(python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' ${HONGGFUZZ_PATH})"
command_line+=("--workspace=${FUZZER_OUTPUT_ROOT}")

if [[ -n "${FUZZER_SEED_CORPUS_DIR}" ]]; then
    command_line+=("--input=${FUZZER_SEED_CORPUS_DIR}")
    command_line+=("--output=${FUZZER_OUTPUT_CORPUS_DIR}")
else
    command_line+=("--input=${FUZZER_OUTPUT_CORPUS_DIR}")
fi
if (( FUZZER_IS_REGRESSION )); then
    # Dry-run-only mode - see https://github.com/google/honggfuzz/issues/296.
    command_line+=("--mutations_per_run=0")
    command_line+=("--verifier")
    # Make the output more suitable for debugging.
    command_line+=("--verbose")
    command_line+=("--keep_output")
fi

command_line+=("--crashdir=${FUZZER_ARTIFACTS_DIR}")

if [[ "${FUZZER_TIMEOUT_SECS}" -gt 0 ]]; then
    command_line+=("--run_time=${FUZZER_TIMEOUT_SECS}")
fi

if [[ -n "${FUZZER_DICTIONARY_PATH}" ]]; then
    command_line+=("--dict=${FUZZER_DICTIONARY_PATH}")
fi
command_line+=("--" "${FUZZER_BINARY}")

exec "${command_line[@]}" "$@"
