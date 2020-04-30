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

#include "extensions/common/json_util.h"

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
absl::optional<std::string> JsonValueAs<std::string>(
    const ::nlohmann::json& j) {
  if (j.is_string()) {
    return j.get<std::string>();
  }
  return absl::nullopt;
}

template <>
absl::optional<bool> JsonValueAs<bool>(const ::nlohmann::json& j) {
  if (j.is_boolean()) {
    return j.get<bool>();
  }
  if (j.is_string()) {
    const std::string& v = j.get_ref<std::string const&>();
    if (v == "true") {
      return true;
    } else if (v == "false") {
      return false;
    }
  }
  return absl::nullopt;
}

bool JsonArrayIterate(
    const ::nlohmann::json& j, absl::string_view field,
    const std::function<bool(const ::nlohmann::json& elt)>& visitor) {
  auto it = j.find(field);
  if (it == j.end()) {
    return true;
  }
  if (!it.value().is_array()) {
    return false;
  }
  for (const auto& elt : it.value().items()) {
    if (!visitor(elt.value())) {
      return false;
    }
  }
  return true;
}

bool JsonObjectIterate(const ::nlohmann::json& j, absl::string_view field,
                       const std::function<bool(std::string key)>& visitor) {
  auto it = j.find(field);
  if (it == j.end()) {
    return true;
  }
  if (!it.value().is_object()) {
    return false;
  }
  for (const auto& elt : it.value().items()) {
    auto key = JsonValueAs<std::string>(elt.key());
    if (!key.has_value()) {
      return false;
    }
    if (!visitor(key.value())) {
      return false;
    }
  }
  return true;
}

}  // namespace Common
}  // namespace Wasm
