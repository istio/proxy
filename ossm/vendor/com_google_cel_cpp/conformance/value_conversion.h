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
//
// Converters to/from serialized Value to/from runtime values.
#ifndef THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_
#define THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "google/protobuf/arena.h"

namespace cel::conformance_internal {

absl::StatusOr<Value> FromConformanceValue(
    ValueManager& value_manager, const google::api::expr::v1alpha1::Value& value);

absl::StatusOr<google::api::expr::v1alpha1::Value> ToConformanceValue(
    ValueManager& value_manager, const Value& value);

absl::StatusOr<Type> FromConformanceType(google::protobuf::Arena* arena,
                                         const google::api::expr::v1alpha1::Type& type);

}  // namespace cel::conformance_internal
#endif  // THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_
