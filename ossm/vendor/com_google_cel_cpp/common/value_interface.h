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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/internal/data_interface.h"
#include "common/json.h"
#include "common/value_kind.h"

namespace cel {

class TypeManager;
class ValueManager;

class ValueInterface : public common_internal::DataInterface {
 public:
  using DataInterface::DataInterface;

  virtual ValueKind kind() const = 0;

  virtual absl::string_view GetTypeName() const = 0;

  virtual std::string DebugString() const = 0;

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  virtual absl::Status SerializeTo(AnyToJsonConverter& converter,
                                   absl::Cord& value) const;

  // `ConvertToJson` converts this value to `Json`. If this value does not
  // support conversion to JSON, `FAILED_PRECONDITION` is returned.
  virtual absl::StatusOr<Json> ConvertToJson(
      AnyToJsonConverter& converter) const;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_
