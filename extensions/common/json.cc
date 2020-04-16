/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include "extensions/common/json.h"

#include "absl/strings/numbers.h"

namespace Wasm {
namespace Common {

::nlohmann::json JsonParse(absl::string_view str) {
  return ::nlohmann::json::parse(str, nullptr, false);
}

template <>
absl::optional<int64_t> JsonValueAs<int64_t>(const ::nlohmann::json& j) {
  if (j.is_number()) {
    return j.get<int64_t>();
  } else if (j.is_string()) {
    int64_t result = 0;
    if (absl::SimpleAtoi(j.get_ref<std::string const&>(), &result)) {
      return result;
    }
  }
  return absl::nullopt;
}

template <>
absl::optional<absl::string_view> JsonValueAs<absl::string_view>(
    const ::nlohmann::json& j) {
  if (j.is_string()) {
    return absl::string_view(j.get_ref<std::string const&>());
  }
  return absl::nullopt;
}

template <>
absl::optional<bool> JsonValueAs<bool>(const ::nlohmann::json& j) {
  if (j.is_boolean()) {
    return j.get<bool>();
  }
  return absl::nullopt;
}

}  // namespace Common
}  // namespace Wasm
