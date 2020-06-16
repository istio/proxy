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

absl::optional<JsonObject> JsonParse(absl::string_view str) {
  const auto result = JsonObject::parse(str, nullptr, false);
  if (result.empty() || result.is_discarded()) {
    return absl::nullopt;
  }
  return result;
}

template <>
std::pair<absl::optional<int64_t>, absl::optional<JsonParseError>>
JsonValueAs<int64_t>(const JsonObject& j) {
  if (j.is_number()) {
    return std::make_pair(j.get<int64_t>(), absl::nullopt);
  } else if (j.is_string()) {
    int64_t result = 0;
    if (absl::SimpleAtoi(j.get_ref<std::string const&>(), &result)) {
      return std::make_pair(result, absl::nullopt);
    }
  }
  return std::make_pair(absl::nullopt,
                        JsonParseError{JsonParserErrorDetail::TYPE_ERROR,
                                       "Type must be string or number"});
}

template <>
std::pair<absl::optional<uint64_t>, absl::optional<JsonParseError>>
JsonValueAs<uint64_t>(const JsonObject& j) {
  if (j.is_number()) {
    return std::make_pair(j.get<uint64_t>(), absl::nullopt);
  } else if (j.is_string()) {
    uint64_t result = 0;
    if (absl::SimpleAtoi(j.get_ref<std::string const&>(), &result)) {
      return std::make_pair(result, absl::nullopt);
    }
  }
  return std::make_pair(absl::nullopt,
                        JsonParseError{JsonParserErrorDetail::TYPE_ERROR,
                                       "Type must be string or number"});
}

template <>
std::pair<absl::optional<absl::string_view>, absl::optional<JsonParseError>>
JsonValueAs<absl::string_view>(const JsonObject& j) {
  if (j.is_string()) {
    return std::make_pair(absl::string_view(j.get_ref<std::string const&>()),
                          absl::nullopt);
  }
  return std::make_pair(
      absl::nullopt,
      JsonParseError{JsonParserErrorDetail::TYPE_ERROR, "Type must be string"});
}

template <>
std::pair<absl::optional<std::string>, absl::optional<JsonParseError>>
JsonValueAs<std::string>(const JsonObject& j) {
  if (j.is_string()) {
    return std::make_pair(j.get<std::string>(), absl::nullopt);
  }
  return std::make_pair(
      absl::nullopt,
      JsonParseError{JsonParserErrorDetail::TYPE_ERROR, "Type must be string"});
}

template <>
std::pair<absl::optional<bool>, absl::optional<JsonParseError>>
JsonValueAs<bool>(const JsonObject& j) {
  if (j.is_boolean()) {
    return std::make_pair(j.get<bool>(), absl::nullopt);
  }
  if (j.is_string()) {
    const std::string& v = j.get_ref<std::string const&>();
    if (v == "true") {
      return std::make_pair(true, absl::nullopt);
    } else if (v == "false") {
      return std::make_pair(false, absl::nullopt);
    }
  }
  return std::make_pair(
      absl::nullopt,
      JsonParseError{JsonParserErrorDetail::TYPE_ERROR,
                     "Type must be boolean or string(true/false)"});
}

template <>
bool JsonArrayIterate(
    const JsonObject& j, absl::string_view field,
    const std::function<bool(const JsonObject& elt)>& visitor) {
  auto it = j.find(field);
  if (it == j.end()) {
    return true;
  }
  if (!it.value().is_array()) {
    return false;
  }
  for (const auto& elt : it.value().items()) {
    assert(elt.value().is_object());
    if (!visitor(elt.value())) {
      return false;
    }
  }
  return true;
}

template <>
bool JsonArrayIterate(
    const JsonObject& j, absl::string_view field,
    const std::function<bool(const std::string& elt)>& visitor) {
  auto it = j.find(field);
  if (it == j.end()) {
    return true;
  }
  if (!it.value().is_array()) {
    return false;
  }
  for (const auto& elt : it.value().items()) {
    assert(elt.value().is_string());
    if (!visitor(elt.value())) {
      return false;
    }
  }
  return true;
}

bool JsonObjectIterate(const JsonObject& j, absl::string_view field,
                       const std::function<bool(std::string key)>& visitor) {
  auto it = j.find(field);
  if (it == j.end()) {
    return true;
  }
  if (!it.value().is_object()) {
    return false;
  }
  for (const auto& elt : it.value().items()) {
    auto result = JsonValueAs<std::string>(elt.key());
    if (!result.first.has_value()) {
      return false;
    }
    if (!visitor(result.first.value())) {
      return false;
    }
  }
  return true;
}

}  // namespace Common
}  // namespace Wasm
