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

#pragma once

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "extensions/common/nlohmann_json.hpp"

/**
 * Utilities for working with JSON without exceptions.
 */
namespace Wasm {
namespace Common {

// Parse JSON. Returns the discarded value if fails.
::nlohmann::json JsonParse(absl::string_view str);

template <typename T>
absl::optional<T> JsonValueAs(const ::nlohmann::json& j);

template <>
absl::optional<absl::string_view> JsonValueAs<absl::string_view>(
    const ::nlohmann::json& j);

template <>
absl::optional<std::string> JsonValueAs<std::string>(const ::nlohmann::json& j);

template <>
absl::optional<int64_t> JsonValueAs<int64_t>(const ::nlohmann::json& j);

template <>
absl::optional<bool> JsonValueAs<bool>(const ::nlohmann::json& j);

template <typename T>
absl::optional<T> JsonGetField(const ::nlohmann::json& j,
                               absl::string_view field) {
  auto it = j.find(field);
  if (it == j.end()) {
    return absl::nullopt;
  }
  return JsonValueAs<T>(it.value());
}

// Iterate over an optional array field.
// Returns false if set and not an array, or any of the visitor calls returns
// false.
bool JsonArrayIterate(
    const ::nlohmann::json& j, absl::string_view field,
    const std::function<bool(const ::nlohmann::json& elt)>& visitor);

// Iterate over an optional object field key set.
// Returns false if set and not an object, or any of the visitor calls returns
// false.
bool JsonObjectIterate(const ::nlohmann::json& j, absl::string_view field,
                       const std::function<bool(std::string key)>& visitor);

}  // namespace Common
}  // namespace Wasm
