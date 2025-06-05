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
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"

namespace google::api::expr::runtime {

// Simple manager for comprehension variables.
//
// At plan time, each comprehension variable is assigned a slot by index.
// This is used instead of looking up the variable identifier by name in a
// runtime stack.
//
// Callers must handle range checking.
class ComprehensionSlots {
 public:
  struct Slot {
    cel::Value value;
    AttributeTrail attribute;
  };

  // Trivial instance if no slots are needed.
  // Trivially thread safe since no effective state.
  static ComprehensionSlots& GetEmptyInstance() {
    static absl::NoDestructor<ComprehensionSlots> instance(0);
    return *instance;
  }

  explicit ComprehensionSlots(size_t size) : size_(size), slots_(size) {}

  // Move only
  ComprehensionSlots(const ComprehensionSlots&) = delete;
  ComprehensionSlots& operator=(const ComprehensionSlots&) = delete;
  ComprehensionSlots(ComprehensionSlots&&) = default;
  ComprehensionSlots& operator=(ComprehensionSlots&&) = default;

  // Return ptr to slot at index.
  // If not set, returns nullptr.
  Slot* Get(size_t index) {
    ABSL_DCHECK_LT(index, slots_.size());
    auto& slot = slots_[index];
    if (!slot.has_value()) return nullptr;
    return &slot.value();
  }

  void Reset() {
    slots_.clear();
    slots_.resize(size_);
  }

  void ClearSlot(size_t index) {
    ABSL_DCHECK_LT(index, slots_.size());
    slots_[index] = absl::nullopt;
  }

  void Set(size_t index) {
    ABSL_DCHECK_LT(index, slots_.size());
    slots_[index].emplace();
  }

  void Set(size_t index, cel::Value value) {
    Set(index, std::move(value), AttributeTrail());
  }

  void Set(size_t index, cel::Value value, AttributeTrail attribute) {
    ABSL_DCHECK_LT(index, slots_.size());
    slots_[index] = Slot{std::move(value), std::move(attribute)};
  }

  size_t size() const { return slots_.size(); }

 private:
  size_t size_;
  std::vector<absl::optional<Slot>> slots_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_
