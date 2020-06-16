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

using JsonObject = ::nlohmann::json;
using JsonObjectValueType = ::nlohmann::detail::value_t;
using JsonParserException = ::nlohmann::detail::exception;
using JsonParserOutOfRangeException = ::nlohmann::detail::out_of_range;
using JsonParserTypeErrorException = ::nlohmann::detail::type_error;

enum JsonParserErrorDetail {
  OUT_OF_RANGE,
  TYPE_ERROR,
  PARSE_ERROR,
};

struct JsonParseError {
  JsonParserErrorDetail error_detail_;
  std::string message_;
};

// Parse JSON. Returns the discarded value if fails.
absl::optional<JsonObject> JsonParse(absl::string_view str);

template <typename T>
std::pair<absl::optional<T>, absl::optional<JsonParseError>> JsonValueAs(
    const JsonObject&) {
  static_assert(true, "Unsupported Type");
}

template <>
std::pair<absl::optional<absl::string_view>, absl::optional<JsonParseError>>
JsonValueAs<absl::string_view>(const JsonObject& j);

template <>
std::pair<absl::optional<std::string>, absl::optional<JsonParseError>>
JsonValueAs<std::string>(const JsonObject& j);

template <>
std::pair<absl::optional<int64_t>, absl::optional<JsonParseError>>
JsonValueAs<int64_t>(const JsonObject& j);

template <>
std::pair<absl::optional<uint64_t>, absl::optional<JsonParseError>>
JsonValueAs<uint64_t>(const JsonObject& j);

template <>
std::pair<absl::optional<bool>, absl::optional<JsonParseError>>
JsonValueAs<bool>(const JsonObject& j);

template <typename T>
std::pair<absl::optional<T>, absl::optional<JsonParseError>> JsonGetField(
    const JsonObject& j, absl::string_view field) {
  auto it = j.find(field);
  if (it == j.end()) {
    return std::make_pair(
        absl::nullopt,
        JsonParseError{JsonParserErrorDetail::OUT_OF_RANGE,
                       "Key " + std::string(field) + " is not found"});
  }
  return JsonValueAs<T>(it.value());
}

// Iterate over an optional array field.
// Returns false if set and not an array, or any of the visitor calls returns
// false.
template <typename T>
bool JsonArrayIterate(const JsonObject&, absl::string_view,
                      const std::function<bool(const T& elt)>&) {
  static_assert(true, "Unsupported type");
}

template <>
bool JsonArrayIterate(
    const JsonObject& j, absl::string_view field,
    const std::function<bool(const JsonObject& elt)>& visitor);

template <>
bool JsonArrayIterate<std::string>(
    const JsonObject& j, absl::string_view field,
    const std::function<bool(const std::string& elt)>& visitor);

// Iterate over an optional object field key set.
// Returns false if set and not an object, or any of the visitor calls returns
// false.
bool JsonObjectIterate(const JsonObject& j, absl::string_view field,
                       const std::function<bool(std::string key)>& visitor);

}  // namespace Common
}  // namespace Wasm
