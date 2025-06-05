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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_BUILDER_H_

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

// Special implementation of list which is both a modern list and legacy list.
// Do not try this at home. This should only be implemented in
// `list_value_builder.cc`.
class CompatListValue : public ParsedListValueInterface,
                        public google::api::expr::runtime::CelList {
 private:
  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<CompatListValue>();
  }
};

absl::Nonnull<const CompatListValue*> EmptyCompatListValue();

absl::StatusOr<absl::Nonnull<const CompatListValue*>> MakeCompatListValue(
    absl::Nonnull<google::protobuf::Arena*> arena, const ParsedListValue& value);

// Extension of ParsedListValueInterface which is also mutable. Accessing this
// like a normal list before all elements are finished being appended is a bug.
// This is primarily used by the runtime to efficiently implement comprehensions
// which accumulate results into a list.
//
// IMPORTANT: This type is only meant to be utilized by the runtime.
class MutableListValue : public ParsedListValueInterface {
 public:
  virtual absl::Status Append(Value value) const = 0;

  virtual void Reserve(size_t capacity) const {}

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<MutableListValue>();
  }
};

// Special implementation of list which is both a modern list, legacy list, and
// mutable.
//
// NOTE: We do not extend CompatListValue to avoid having to use virtual
// inheritance and `dynamic_cast`.
class MutableCompatListValue : public MutableListValue,
                               public google::api::expr::runtime::CelList {
 private:
  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<MutableCompatListValue>();
  }
};

Shared<MutableListValue> NewMutableListValue(Allocator<> allocator);

bool IsMutableListValue(const Value& value);
bool IsMutableListValue(const ListValue& value);

absl::Nullable<const MutableListValue*> AsMutableListValue(
    const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND);
absl::Nullable<const MutableListValue*> AsMutableListValue(
    const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

const MutableListValue& GetMutableListValue(
    const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND);
const MutableListValue& GetMutableListValue(
    const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

absl::Nonnull<cel::ListValueBuilderPtr> NewListValueBuilder(
    ValueFactory& value_factory);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_BUILDER_H_
