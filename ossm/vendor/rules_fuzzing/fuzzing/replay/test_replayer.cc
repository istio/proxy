// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fuzzing/replay/test_replayer.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "fuzzing/replay/file_util.h"

namespace fuzzing {

absl::Status TestReplayer::ReplayTestData(absl::string_view test) {
  std::unique_ptr<uint8_t[]> test_copy(new uint8_t[test.size()]);
  memcpy(test_copy.get(), test.data(), test.size());
  const int result = callback_(test_copy.get(), test.size());
  if (result == 0) {
    return absl::OkStatus();
  } else {
    return absl::InternalError(absl::StrCat("LLVMFuzzerTestOneInput returned ",
                                            result, " instead of 0"));
  }
}

absl::Status TestReplayer::ReplayTestFile(absl::string_view path) {
  absl::Status status = test_file_buffer_.ReadFile(path);
  status.Update(ReplayTestData(test_file_buffer_.last_test()));
  return status;
}

absl::Status TestReplayer::ReplayTests(absl::string_view path) {
  absl::Status replay_status;
  const absl::Status yield_status =
      YieldFiles(path, [this, &replay_status](absl::string_view file_path,
                                              const struct stat& file_stat) {
        if (S_ISREG(file_stat.st_mode)) {
          const absl::Status status = ReplayTestFile(std::string(file_path));
          absl::FPrintF(stderr, "Replaying '%s' (%zd bytes): %s\n", file_path,
                        file_stat.st_size, status.ToString());
          replay_status.Update(status);
        } else {
          absl::FPrintF(stderr, "Replaying '%s': SKIPPED (not a file)\n",
                        file_path);
        }
      });
  replay_status.Update(yield_status);
  return replay_status;
}

}  // namespace fuzzing
