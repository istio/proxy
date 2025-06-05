// Copyright 2014 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef BAZEL_SRC_MAIN_CPP_UTIL_NUMBERS_H_
#define BAZEL_SRC_MAIN_CPP_UTIL_NUMBERS_H_

#include <cstdint>
#include <string>

namespace blaze_util {

bool safe_strto32(const std::string &text, int *value);

int32_t strto32(const char *str, char **endptr, int base);

}  // namespace blaze_util

#endif  // BAZEL_SRC_MAIN_CPP_UTIL_NUMBERS_H_
