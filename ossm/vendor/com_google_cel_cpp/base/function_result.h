// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_H_

#include <cstdint>
#include <utility>

#include "base/function_descriptor.h"

namespace cel {

// Represents a function result that is unknown at the time of execution. This
// allows for lazy evaluation of expensive functions.
class FunctionResult final {
 public:
  FunctionResult() = delete;
  FunctionResult(const FunctionResult&) = default;
  FunctionResult(FunctionResult&&) = default;
  FunctionResult& operator=(const FunctionResult&) = default;
  FunctionResult& operator=(FunctionResult&&) = default;

  FunctionResult(FunctionDescriptor descriptor, int64_t expr_id)
      : descriptor_(std::move(descriptor)), expr_id_(expr_id) {}

  // The descriptor of the called function that return Unknown.
  const FunctionDescriptor& descriptor() const { return descriptor_; }

  // The id of the |Expr| that triggered the function call step. Provided
  // informationally -- if two different |Expr|s generate the same unknown call,
  // they will be treated as the same unknown function result.
  int64_t call_expr_id() const { return expr_id_; }

  // Equality operator provided for testing. Compatible with set less-than
  // comparator.
  // Compares descriptor then arguments elementwise.
  bool IsEqualTo(const FunctionResult& other) const {
    return descriptor() == other.descriptor();
  }

  // TODO: re-implement argument capture

 private:
  FunctionDescriptor descriptor_;
  int64_t expr_id_;
};

inline bool operator==(const FunctionResult& lhs, const FunctionResult& rhs) {
  return lhs.IsEqualTo(rhs);
}

inline bool operator<(const FunctionResult& lhs, const FunctionResult& rhs) {
  return lhs.descriptor() < rhs.descriptor();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_H_
