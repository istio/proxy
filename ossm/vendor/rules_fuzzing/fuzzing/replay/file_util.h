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

// Utilities for working with files and filesystems.

#ifndef FUZZING_REPLAY_FILE_UTIL_H_
#define FUZZING_REPLAY_FILE_UTIL_H_

#include <sys/stat.h>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace fuzzing {

// Recursively traverses the directory at `path` and calls the provided
// `callback` for each file encountered. The callback receives the file path and
// its stat structure as arguments. Returns OK if the entire directory tree was
// traversed successfully, or an error status if some parts could not be
// traversed. If `path` refers to a file, the callback will be called once and
// the function returns OK.
absl::Status YieldFiles(
    absl::string_view path,
    absl::FunctionRef<void(absl::string_view, const struct stat&)> callback);

// Opens the given `path` for writing and sets the file contents to `contents`.
absl::Status SetFileContents(absl::string_view path,
                             absl::string_view contents);

}  // namespace fuzzing

#endif  // FUZZING_REPLAY_FILE_UTIL_H_
