// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_

#include <cstddef>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/fixed_array.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"

namespace google::api::expr::runtime {

class ComprehensionSlot final {
 public:
  ComprehensionSlot() = default;
  ComprehensionSlot(const ComprehensionSlot&) = delete;
  ComprehensionSlot(ComprehensionSlot&&) = delete;
  ComprehensionSlot& operator=(const ComprehensionSlot&) = delete;
  ComprehensionSlot& operator=(ComprehensionSlot&&) = delete;

  const cel::Value& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Has());

    return value_;
  }

  cel::Value* absl_nonnull mutable_value() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Has());

    return &value_;
  }

  const AttributeTrail& attribute() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Has());

    return attribute_;
  }

  AttributeTrail* absl_nonnull mutable_attribute()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Has());

    return &attribute_;
  }

  bool Has() const { return has_; }

  void Set() { Set(cel::NullValue(), absl::nullopt); }

  template <typename V>
  void Set(V&& value) {
    Set(std::forward<V>(value), absl::nullopt);
  }

  template <typename V, typename A>
  void Set(V&& value, A&& attribute) {
    value_ = std::forward<V>(value);
    attribute_ = std::forward<A>(attribute);
    has_ = true;
  }

  void Clear() {
    if (has_) {
      value_ = cel::NullValue();
      attribute_ = absl::nullopt;
      has_ = false;
    }
  }

 private:
  cel::Value value_;
  AttributeTrail attribute_;
  bool has_ = false;
};

// Simple manager for comprehension variables.
//
// At plan time, each comprehension variable is assigned a slot by index.
// This is used instead of looking up the variable identifier by name in a
// runtime stack.
//
// Callers must handle range checking.
class ComprehensionSlots final {
 public:
  using Slot = ComprehensionSlot;

  // Trivial instance if no slots are needed.
  // Trivially thread safe since no effective state.
  static ComprehensionSlots& GetEmptyInstance() {
    static absl::NoDestructor<ComprehensionSlots> instance(0);
    return *instance;
  }

  explicit ComprehensionSlots(size_t size) : slots_(size) {}

  ComprehensionSlots(const ComprehensionSlots&) = delete;
  ComprehensionSlots& operator=(const ComprehensionSlots&) = delete;

  ComprehensionSlots(ComprehensionSlots&&) = delete;
  ComprehensionSlots& operator=(ComprehensionSlots&&) = delete;

  Slot* absl_nonnull Get(size_t index) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_LT(index, size());

    return &slots_[index];
  }

  void Reset() {
    for (Slot& slot : slots_) {
      slot.Clear();
    }
  }

  void ClearSlot(size_t index) { Get(index)->Clear(); }

  template <typename V>
  void Set(size_t index, V&& value) {
    Set(index, std::forward<V>(value), absl::nullopt);
  }

  template <typename V, typename A>
  void Set(size_t index, V&& value, A&& attribute) {
    Get(index)->Set(std::forward<V>(value), std::forward<A>(attribute));
  }

  size_t size() const { return slots_.size(); }

 private:
  absl::FixedArray<ComprehensionSlot, 0> slots_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_
