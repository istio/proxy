// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ATTRIBUTE_MATCHER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ATTRIBUTE_MATCHER_H_

#include "base/attribute.h"

namespace cel::runtime_internal {

// Interface for matching unknown and missing attributes against the
// observed attribute trail at runtime.
class AttributeMatcher {
 public:
  using MatchResult = cel::AttributePattern::MatchType;

  virtual ~AttributeMatcher() = default;

  // Checks whether the attribute trail matches any unknown patterns.
  // Used to identify and collect referenced unknowns in an UnknownValue.
  virtual MatchResult CheckForUnknown(const Attribute& attr) const {
    return MatchResult::NONE;
  };

  // Checks whether the attribute trail matches any missing patterns.
  // Used to identify missing attributes, and report an error if referenced
  // directly.
  virtual MatchResult CheckForMissing(const Attribute& attr) const {
    return MatchResult::NONE;
  };
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_ATTRIBUTE_MATCHER_H_
