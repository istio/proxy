// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_

#include <cstddef>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/value_interface.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ListValue;

class ListValueInterface : public ValueInterface {
 public:
  using alternative_type = ListValue;

  static constexpr ValueKind kKind = ValueKind::kList;

  ValueKind kind() const final { return kKind; }

  absl::string_view GetTypeName() const final { return "list"; }

  absl::StatusOr<Json> ConvertToJson(
      AnyToJsonConverter& converter) const final {
    return ConvertToJsonArray(converter);
  }

  virtual absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const = 0;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;

  using ForEachWithIndexCallback =
      absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)>;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_
