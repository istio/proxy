// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_

#include <cstddef>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/public/cel_value.h"
#include "google/protobuf/arena.h"

namespace cel {

class ValueFactory;

namespace common_internal {

// Special implementation of map which is both a modern map and legacy map. Do
// not try this at home. This should only be implemented in
// `map_value_builder.cc`.
class CompatMapValue : public ParsedMapValueInterface,
                       public google::api::expr::runtime::CelMap {
 private:
  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<CompatMapValue>();
  }
};

absl::Nonnull<const CompatMapValue*> EmptyCompatMapValue();

absl::StatusOr<absl::Nonnull<const CompatMapValue*>> MakeCompatMapValue(
    absl::Nonnull<google::protobuf::Arena*> arena, const ParsedMapValue& value);

// Extension of ParsedMapValueInterface which is also mutable. Accessing this
// like a normal map before all entries are finished being inserted is a bug.
// This is primarily used by the runtime to efficiently implement comprehensions
// which accumulate results into a map.
//
// IMPORTANT: This type is only meant to be utilized by the runtime.
class MutableMapValue : public ParsedMapValueInterface {
 public:
  virtual absl::Status Put(Value key, Value value) const = 0;

  virtual void Reserve(size_t capacity) const {}

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<MutableMapValue>();
  }
};

// Special implementation of map which is both a modern map, legacy map, and
// mutable.
//
// NOTE: We do not extend CompatMapValue to avoid having to use virtual
// inheritance and `dynamic_cast`.
class MutableCompatMapValue : public MutableMapValue,
                              public google::api::expr::runtime::CelMap {
 private:
  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<MutableCompatMapValue>();
  }
};

Shared<MutableMapValue> NewMutableMapValue(Allocator<> allocator);

bool IsMutableMapValue(const Value& value);
bool IsMutableMapValue(const MapValue& value);

absl::Nullable<const MutableMapValue*> AsMutableMapValue(
    const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND);
absl::Nullable<const MutableMapValue*> AsMutableMapValue(
    const MapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

const MutableMapValue& GetMutableMapValue(
    const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND);
const MutableMapValue& GetMutableMapValue(
    const MapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

absl::Nonnull<cel::MapValueBuilderPtr> NewMapValueBuilder(
    ValueFactory& value_factory);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_
