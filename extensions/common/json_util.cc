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
  if (result.is_discarded() || !result.is_object()) {
    return absl::nullopt;
  }
  return result;
}

template <>
std::pair<absl::optional<int64_t>, JsonParserResultDetail> JsonValueAs<int64_t>(
    const JsonObject& j) {
  if (j.is_number()) {
    return std::make_pair(j.get<int64_t>(), JsonParserResultDetail::OK);
  } else if (j.is_string()) {
    int64_t result = 0;
    if (absl::SimpleAtoi(j.get_ref<std::string const&>(), &result)) {
      return std::make_pair(result, JsonParserResultDetail::OK);
    } else {
      return std::make_pair(absl::nullopt,
                            JsonParserResultDetail::INVALID_VALUE);
    }
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

template <>
std::pair<absl::optional<uint64_t>, JsonParserResultDetail>
JsonValueAs<uint64_t>(const JsonObject& j) {
  if (j.is_number()) {
    return std::make_pair(j.get<uint64_t>(), JsonParserResultDetail::OK);
  } else if (j.is_string()) {
    uint64_t result = 0;
    if (absl::SimpleAtoi(j.get_ref<std::string const&>(), &result)) {
      return std::make_pair(result, JsonParserResultDetail::OK);
    } else {
      return std::make_pair(absl::nullopt,
                            JsonParserResultDetail::INVALID_VALUE);
    }
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

template <>
std::pair<absl::optional<absl::string_view>, JsonParserResultDetail>
JsonValueAs<absl::string_view>(const JsonObject& j) {
  if (j.is_string()) {
    return std::make_pair(absl::string_view(j.get_ref<std::string const&>()),
                          JsonParserResultDetail::OK);
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

template <>
std::pair<absl::optional<std::string>, JsonParserResultDetail>
JsonValueAs<std::string>(const JsonObject& j) {
  if (j.is_string()) {
    return std::make_pair(j.get_ref<std::string const&>(),
                          JsonParserResultDetail::OK);
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

template <>
std::pair<absl::optional<bool>, JsonParserResultDetail> JsonValueAs<bool>(
    const JsonObject& j) {
  if (j.is_boolean()) {
    return std::make_pair(j.get<bool>(), JsonParserResultDetail::OK);
  }
  if (j.is_string()) {
    const std::string& v = j.get_ref<std::string const&>();
    if (v == "true") {
      return std::make_pair(true, JsonParserResultDetail::OK);
    } else if (v == "false") {
      return std::make_pair(false, JsonParserResultDetail::OK);
    } else {
      return std::make_pair(absl::nullopt,
                            JsonParserResultDetail::INVALID_VALUE);
    }
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

template <>
std::pair<absl::optional<std::vector<absl::string_view>>,
          JsonParserResultDetail>
JsonValueAs<std::vector<absl::string_view>>(const JsonObject& j) {
  std::pair<absl::optional<std::vector<absl::string_view>>,
            JsonParserResultDetail>
      values = std::make_pair(absl::nullopt, JsonParserResultDetail::OK);
  if (j.is_array()) {
    for (const auto& elt : j) {
      if (!elt.is_string()) {
        values.first = absl::nullopt;
        values.second = JsonParserResultDetail::TYPE_ERROR;
        return values;
      }
      if (!values.first.has_value()) {
        values.first = std::vector<absl::string_view>();
      }
      values.first->emplace_back(elt.get_ref<std::string const&>());
    }
    return values;
  }
  values.second = JsonParserResultDetail::TYPE_ERROR;
  return values;
}

template <>
std::pair<absl::optional<JsonObject>, JsonParserResultDetail>
JsonValueAs<JsonObject>(const JsonObject& j) {
  if (j.is_object()) {
    return std::make_pair(j.get<JsonObject>(), JsonParserResultDetail::OK);
  }
  return std::make_pair(absl::nullopt, JsonParserResultDetail::TYPE_ERROR);
}

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
    auto json_value = JsonValueAs<std::string>(elt.key());
    if (json_value.second != JsonParserResultDetail::OK) {
      return false;
    }
    if (!visitor(json_value.first.value())) {
      return false;
    }
  }
  return true;
}

bool JsonObjectIterate(const JsonObject& j,
                       const std::function<bool(std::string key)>& visitor) {
  for (const auto& elt : j.items()) {
    auto json_value = JsonValueAs<std::string>(elt.key());
    if (json_value.second != JsonParserResultDetail::OK) {
      return false;
    }
    if (!visitor(json_value.first.value())) {
      return false;
    }
  }
  return true;
}

}  // namespace Common
}  // namespace Wasm
