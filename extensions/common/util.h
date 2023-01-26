/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

#include "absl/strings/string_view.h"

namespace Wasm {
namespace Common {

// None response flag.
const char NONE[] = "-";

// Parses an integer response flag into a readable short string.
const std::string parseResponseFlag(uint64_t response_flag);

// Used for converting sanctioned uses of std string_view (e.g. extensions) to
// absl::string_view for internal use.
inline absl::string_view toAbslStringView(std::string_view view) {
  return absl::string_view(view.data(), view.size());
}

// Used for converting internal absl::string_view to sanctioned uses of std
// string_view (e.g. extensions).
inline std::string_view toStdStringView(absl::string_view view) {
  return std::string_view(view.data(), view.size());
}

} // namespace Common
} // namespace Wasm
