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

// Utilities for working with absl::Status values.

#ifndef FUZZING_REPLAY_STATUS_UTIL_H_
#define FUZZING_REPLAY_STATUS_UTIL_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace fuzzing {

// Creates an error status value that includes the given `message` and a
// description of the `errno_value`. Returns OK if `errno_value` is zero.
absl::Status ErrnoStatus(absl::string_view message, int errno_value);

}  // namespace fuzzing

#endif  // FUZZING_REPLAY_STATUS_UTIL_H_
