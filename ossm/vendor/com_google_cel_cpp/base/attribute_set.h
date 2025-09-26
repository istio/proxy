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

#ifndef THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_SET_H_
#define THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_SET_H_

#include "absl/container/btree_set.h"
#include "absl/types/span.h"
#include "base/attribute.h"

namespace google::api::expr::runtime {
class AttributeUtility;
}  // namespace google::api::expr::runtime

namespace cel {

class UnknownValue;
namespace base_internal {
class UnknownSet;
}

// AttributeSet is a container for CEL attributes that are identified as
// unknown during expression evaluation.
class AttributeSet final {
 private:
  using Container = absl::btree_set<Attribute>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using iterator = typename Container::const_iterator;
  using const_iterator = typename Container::const_iterator;

  AttributeSet() = default;
  AttributeSet(const AttributeSet&) = default;
  AttributeSet(AttributeSet&&) = default;
  AttributeSet& operator=(const AttributeSet&) = default;
  AttributeSet& operator=(AttributeSet&&) = default;

  explicit AttributeSet(absl::Span<const Attribute> attributes) {
    for (const auto& attr : attributes) {
      Add(attr);
    }
  }

  AttributeSet(const AttributeSet& set1, const AttributeSet& set2)
      : attributes_(set1.attributes_) {
    for (const auto& attr : set2.attributes_) {
      Add(attr);
    }
  }

  iterator begin() const { return attributes_.begin(); }

  const_iterator cbegin() const { return attributes_.cbegin(); }

  iterator end() const { return attributes_.end(); }

  const_iterator cend() const { return attributes_.cend(); }

  size_type size() const { return attributes_.size(); }

  bool empty() const { return attributes_.empty(); }

  bool operator==(const AttributeSet& other) const {
    return this == &other || attributes_ == other.attributes_;
  }

  bool operator!=(const AttributeSet& other) const {
    return !operator==(other);
  }

  static AttributeSet Merge(const AttributeSet& set1,
                            const AttributeSet& set2) {
    return AttributeSet(set1, set2);
  }

 private:
  friend class google::api::expr::runtime::AttributeUtility;
  friend class UnknownValue;
  friend class base_internal::UnknownSet;

  void Add(const Attribute& attribute) { attributes_.insert(attribute); }

  void Add(const AttributeSet& other) {
    for (const auto& attribute : other) {
      Add(attribute);
    }
  }

  // Attribute container.
  Container attributes_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_ATTRIBUTE_SET_H_
