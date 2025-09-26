// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_

#include <cstddef>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "common/value.h"

namespace cel::runtime_internal {

class IteratorStack final {
 public:
  explicit IteratorStack(size_t max_size) : max_size_(max_size) {
    iterators_.reserve(max_size_);
  }

  IteratorStack(const IteratorStack&) = delete;
  IteratorStack(IteratorStack&&) = delete;

  IteratorStack& operator=(const IteratorStack&) = delete;
  IteratorStack& operator=(IteratorStack&&) = delete;

  size_t size() const { return iterators_.size(); }

  bool empty() const { return iterators_.empty(); }

  bool full() const { return iterators_.size() == max_size_; }

  size_t max_size() const { return max_size_; }

  void Clear() { iterators_.clear(); }

  void Push(absl_nonnull ValueIteratorPtr iterator) {
    ABSL_DCHECK(!full());
    ABSL_DCHECK(iterator != nullptr);

    iterators_.push_back(std::move(iterator));
  }

  ValueIterator* absl_nonnull Peek() {
    ABSL_DCHECK(!empty());
    ABSL_DCHECK(iterators_.back() != nullptr);

    return iterators_.back().get();
  }

  void Pop() {
    ABSL_DCHECK(!empty());

    iterators_.pop_back();
  }

 private:
  std::vector<absl_nonnull ValueIteratorPtr> iterators_;
  size_t max_size_;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_
