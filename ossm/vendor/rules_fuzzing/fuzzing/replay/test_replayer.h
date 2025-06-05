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

#ifndef FUZZING_REPLAY_TEST_REPLAYER_H_
#define FUZZING_REPLAY_TEST_REPLAYER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fuzzing/replay/test_file_buffer.h"

namespace fuzzing {

// Replays in sequence a collection of test files by calling a callback function
// on the contents of each test file.
class TestReplayer {
 public:
  // Creates a new test replayer instance configured to invoke `callback` for
  // each test file being replayed. The callback has the signature and expected
  // semantics of the standard `LLVMFuzzerTestOneInput` entry point.
  // `max_test_file_size` specifies the maximum test size allowed.
  TestReplayer(std::function<int(const uint8_t*, size_t)> callback,
               size_t max_test_file_size)
      : callback_(std::move(callback)), test_file_buffer_(max_test_file_size) {}

  TestReplayer(const TestReplayer&) = delete;
  TestReplayer& operator=(const TestReplayer&) = delete;

  // Replays all the test files found under `path`. The path may point to a file
  // or a directory. Directories are traversed recursively and all files
  // encountered are replayed. The contents of each test file are read in memory
  // and passed to the callback for execution.
  //
  // The files traversed and the result of each replay is printed to stderr.
  //
  // Returns OK if all files were traversed and replayed successfully, or an
  // error status if an error was encountered. The traversal is best-effort and
  // does not stop at the first error encountered.
  absl::Status ReplayTests(absl::string_view path);

 private:
  absl::Status ReplayTestData(absl::string_view test);
  absl::Status ReplayTestFile(absl::string_view path);
  absl::Status ReplayTestDirectory(absl::string_view path);

  const std::function<int(const uint8_t*, size_t)> callback_;
  TestFileBuffer test_file_buffer_;
};

}  // namespace fuzzing

#endif  // FUZZING_REPLAY_TEST_REPLAYER_H_
