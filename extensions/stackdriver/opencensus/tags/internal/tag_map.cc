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

#include "opencensus/tags/tag_map.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "opencensus/common/internal/hash_mix.h"
#include "opencensus/tags/tag_key.h"

namespace opencensus {
namespace tags {

TagMap::TagMap(
    std::initializer_list<std::pair<TagKey, absl::string_view>> tags) {
  tags_.reserve(tags.size());
  for (const auto& tag : tags) {
    tags_.emplace_back(tag.first, std::string(tag.second));
  }
  Initialize();
}

TagMap::TagMap(std::vector<std::pair<TagKey, std::string>> tags)
    : tags_(std::move(tags)) {
  Initialize();
}

void TagMap::Initialize() {
  std::sort(tags_.begin(), tags_.end());

  std::hash<std::string> hasher;
  common::HashMix mixer;
  for (const auto& tag : tags_) {
    mixer.Mix(tag.first.hash());
    mixer.Mix(hasher(tag.second));
  }
  hash_ = mixer.get();
}

std::size_t TagMap::Hash::operator()(const TagMap& tags) const {
  return tags.hash_;
}

bool TagMap::operator==(const TagMap& other) const {
  return tags_ == other.tags_;
}

std::string TagMap::DebugString() const {
  return absl::StrCat(
      "{",
      absl::StrJoin(
          tags_, ", ",
          [](std::string* o, std::pair<const TagKey&, const std::string&> kv) {
            absl::StrAppend(o, "\"", kv.first.name(), "\": \"", kv.second,
                            "\"");
          }),
      "}");
}

}  // namespace tags
}  // namespace opencensus
