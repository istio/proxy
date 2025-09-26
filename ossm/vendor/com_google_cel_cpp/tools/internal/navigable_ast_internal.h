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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_

#include "absl/types/span.h"

namespace cel::tools_internal {

// Implementation for range used for traversals backed by an absl::Span.
//
// This is intended to abstract the metadata layout from clients using the
// traversal methods in navigable_expr.h
//
// RangeTraits provide type info needed to construct the span and adapt to the
// range element type.
template <class RangeTraits>
class SpanRange {
 private:
  using UnderlyingType = typename RangeTraits::UnderlyingType;
  using SpanType = absl::Span<const UnderlyingType>;

  class SpanForwardIter {
   public:
    SpanForwardIter(SpanType span, int i) : i_(i), span_(span) {}

    decltype(RangeTraits::Adapt(SpanType()[0])) operator*() const {
      ABSL_CHECK(i_ < span_.size());
      return RangeTraits::Adapt(span_[i_]);
    }

    SpanForwardIter& operator++() {
      ++i_;
      return *this;
    }

    bool operator==(const SpanForwardIter& other) const {
      return i_ == other.i_ && span_ == other.span_;
    }

    bool operator!=(const SpanForwardIter& other) const {
      return !(*this == other);
    }

   private:
    int i_;
    SpanType span_;
  };

 public:
  explicit SpanRange(SpanType span) : span_(span) {}

  SpanForwardIter begin() { return SpanForwardIter(span_, 0); }

  SpanForwardIter end() { return SpanForwardIter(span_, span_.size()); }

 private:
  SpanType span_;
};

}  // namespace cel::tools_internal

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_
