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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_TYPE_REFLECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_TYPE_REFLECTOR_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/types/thread_compatible_type_introspector.h"
#include "common/value.h"

namespace cel {

class ValueFactory;

namespace common_internal {

class ThreadCompatibleTypeReflector : public ThreadCompatibleTypeIntrospector,
                                      public TypeReflector {
 public:
  ThreadCompatibleTypeReflector() : ThreadCompatibleTypeIntrospector() {}

  absl::StatusOr<absl::Nullable<StructValueBuilderPtr>> NewStructValueBuilder(
      ValueFactory& value_factory, const StructType& type) const override;

  absl::StatusOr<bool> FindValue(ValueFactory& value_factory,
                                 absl::string_view name,
                                 Value& result) const override;
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_TYPE_REFLECTOR_H_
