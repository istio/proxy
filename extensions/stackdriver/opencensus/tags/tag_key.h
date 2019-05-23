// Copyright 2018, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENCENSUS_TAGS_TAG_KEY_H_
#define OPENCENSUS_TAGS_TAG_KEY_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"

namespace opencensus {
namespace tags {

// TagKey is a lightweight, immutable representation of a tag key. It has a
// trivial destructor and can be safely used as a local static variable.
// TagKey is thread-safe.
class TagKey final {
 public:
  // Registers a tag key with 'name'. Registering the same name twice produces
  // equal TagKeys.
  static TagKey Register(absl::string_view name);

  const std::string& name() const;

  bool operator==(TagKey other) const { return id_ == other.id_; }
  bool operator!=(TagKey other) const { return id_ != other.id_; }
  bool operator<(TagKey other) const { return id_ < other.id_; }

  // Returns a suitable hash of the TagKey. The implementation may change.
  std::size_t hash() const { return id_; }

 private:
  friend class TagKeyRegistry;
  explicit TagKey(uint64_t id) : id_(id) {}

  uint64_t id_;
};

}  // namespace tags
}  // namespace opencensus

#endif  // OPENCENSUS_TAGS_TAG_KEY_H_
