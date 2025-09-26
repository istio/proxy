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

#include "fuzzing/replay/test_file_buffer.h"

#include <cassert>
#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "fuzzing/replay/status_util.h"

namespace fuzzing {

TestFileBuffer::TestFileBuffer(size_t max_size)
    : max_size_(max_size), last_size_(0) {
  assert(max_size > 0 && "max_size must be positive");
  buffer_.reset(new char[max_size]);
}

absl::Status TestFileBuffer::ReadFile(absl::string_view path) {
  FILE* f = fopen(std::string(path).c_str(), "r");
  if (!f) {
    last_size_ = 0;
    return ErrnoStatus(absl::StrCat("could not open test file ", path), errno);
  }
  last_size_ = fread(buffer_.get(), 1, max_size_, f);
  absl::Status status = absl::OkStatus();
  if (ferror(f)) {
    status =
        ErrnoStatus(absl::StrCat("could not read test file ", path), errno);
  } else if (!feof(f)) {
    status = absl::ResourceExhaustedError(
        absl::StrCat("file too large (max size ", max_size_, ")"));
  }
  fclose(f);
  return status;
}

}  // namespace fuzzing
