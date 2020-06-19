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

enum JsonParserResultDetail {
  EMPTY,
  OK,
  OUT_OF_RANGE,
  TYPE_ERROR,
  PARSE_ERROR,
  INVALID_VALUE,
};

class JsonParser {
 public:
  void parse(absl::string_view str);
  JsonObject object() { return object_; };
  const JsonParserResultDetail& detail() { return detail_; }

 private:
  void reset();

  JsonParserResultDetail detail_{JsonParserResultDetail::EMPTY};
  JsonObject object_{};
};

template <typename T>
std::pair<absl::optional<T>, JsonParserResultDetail> JsonValueAs(
    const JsonObject&) {
  static_assert(true, "Unsupported Type");
}

template <>
std::pair<absl::optional<absl::string_view>, JsonParserResultDetail>
JsonValueAs<absl::string_view>(const JsonObject& j);

template <>
std::pair<absl::optional<std::string>, JsonParserResultDetail>
JsonValueAs<std::string>(const JsonObject& j);

template <>
std::pair<absl::optional<int64_t>, JsonParserResultDetail> JsonValueAs<int64_t>(
    const JsonObject& j);

template <>
std::pair<absl::optional<uint64_t>, JsonParserResultDetail>
JsonValueAs<uint64_t>(const JsonObject& j);

template <>
std::pair<absl::optional<bool>, JsonParserResultDetail> JsonValueAs<bool>(
    const JsonObject& j);

template <class T>
class JsonGetField {
 public:
  JsonGetField(const JsonObject& j, absl::string_view field);
  const JsonParserResultDetail& detail() { return detail_; }
  T fetch() { return object_; }
  T fetch_or(T v) {
    if (detail_ != JsonParserResultDetail::OK)
      return v;
    else
      return object_;
  };

 private:
  JsonParserResultDetail detail_;
  T object_;
};

template <class T>
JsonGetField<T>::JsonGetField(const JsonObject& j, absl::string_view field) {
  auto it = j.find(field);
  if (it == j.end()) {
    detail_ = JsonParserResultDetail::OUT_OF_RANGE;
    return;
  }
  auto value = JsonValueAs<T>(it.value());
  detail_ = value.second;
  if (value.first.has_value()) {
    object_ = value.first.value();
  }
}

// Iterate over an optional array field.
// Returns false if set and not an array, or any of the visitor calls returns
// false.
bool JsonArrayIterate(
    const JsonObject& j, absl::string_view field,
    const std::function<bool(const JsonObject& elt)>& visitor);

// Iterate over an optional object field key set.
// Returns false if set and not an object, or any of the visitor calls returns
// false.
bool JsonObjectIterate(const JsonObject& j, absl::string_view field,
                       const std::function<bool(std::string key)>& visitor);

}  // namespace Common
}  // namespace Wasm
