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

#include "base/function_descriptor.h"

#include <algorithm>
#include <cstddef>

#include "absl/base/macros.h"
#include "absl/types/span.h"
#include "base/kind.h"

namespace cel {

bool FunctionDescriptor::ShapeMatches(bool receiver_style,
                                      absl::Span<const Kind> types) const {
  if (this->receiver_style() != receiver_style) {
    return false;
  }

  if (this->types().size() != types.size()) {
    return false;
  }

  for (size_t i = 0; i < this->types().size(); i++) {
    Kind this_type = this->types()[i];
    Kind other_type = types[i];
    if (this_type != Kind::kAny && other_type != Kind::kAny &&
        this_type != other_type) {
      return false;
    }
  }
  return true;
}

bool FunctionDescriptor::operator==(const FunctionDescriptor& other) const {
  return impl_.get() == other.impl_.get() ||
         (name() == other.name() &&
          receiver_style() == other.receiver_style() &&
          types().size() == other.types().size() &&
          std::equal(types().begin(), types().end(), other.types().begin()));
}

bool FunctionDescriptor::operator<(const FunctionDescriptor& other) const {
  if (impl_.get() == other.impl_.get()) {
    return false;
  }
  if (name() < other.name()) {
    return true;
  }
  if (name() != other.name()) {
    return false;
  }
  if (receiver_style() < other.receiver_style()) {
    return true;
  }
  if (receiver_style() != other.receiver_style()) {
    return false;
  }
  auto lhs_begin = types().begin();
  auto lhs_end = types().end();
  auto rhs_begin = other.types().begin();
  auto rhs_end = other.types().end();
  while (lhs_begin != lhs_end && rhs_begin != rhs_end) {
    if (*lhs_begin < *rhs_begin) {
      return true;
    }
    if (!(*lhs_begin == *rhs_begin)) {
      return false;
    }
    lhs_begin++;
    rhs_begin++;
  }
  if (lhs_begin == lhs_end && rhs_begin == rhs_end) {
    // Neither has any elements left, they are equal.
    return false;
  }
  if (lhs_begin == lhs_end) {
    // Left has no more elements. Right is greater.
    return true;
  }
  // Right has no more elements. Left is greater.
  ABSL_ASSERT(rhs_begin == rhs_end);
  return false;
}

}  // namespace cel
