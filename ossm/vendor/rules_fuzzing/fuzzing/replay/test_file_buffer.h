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

#ifndef FUZZING_REPLAY_TEST_FILE_BUFFER_H_
#define FUZZING_REPLAY_TEST_FILE_BUFFER_H_

#include <cstddef>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace fuzzing {

// A memory buffer for reading and storing test data from files.
//
// This class permits the use of a single buffer to store the data from multiple
// test files read sequentially, resulting in reduced memory churn for large
// corpora consisting of multiple files.
class TestFileBuffer {
 public:
  // Creates a new buffer holding up to `max_size` bytes.
  explicit TestFileBuffer(size_t max_size);

  TestFileBuffer(const TestFileBuffer&) = delete;
  TestFileBuffer& operator=(const TestFileBuffer&) = delete;

  // Attempts to read the contents of the file at `path`.
  //
  // Possible returned statuses:
  //  * OK, if the entire file was read successfully.
  //  * RESOURCE_EXHAUSTED, if the buffer could not fit the entire file. In
  //    that case, only `max_size` bytes are read from the file.
  //  * other error status, if for some reason the file could not be read.
  //
  // The (possibly partial or truncated) contents read from the file are
  // available through the `last_test()` accessor until the next invocation of
  // this method.
  absl::Status ReadFile(absl::string_view path);

  // Returns the file contents read from the last invocation of `ReadFile`, or
  // an empty string if the buffer has not been used.
  absl::string_view last_test() const {
    return absl::string_view(buffer_.get(), last_size_);
  }

 private:
  const size_t max_size_;
  size_t last_size_;
  std::unique_ptr<char[]> buffer_;
};

}  // namespace fuzzing

#endif  // FUZZING_REPLAY_TEST_FILE_BUFFER_H_
