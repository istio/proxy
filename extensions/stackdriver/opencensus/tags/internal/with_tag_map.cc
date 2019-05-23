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

#include "opencensus/tags/with_tag_map.h"

#include <utility>

#include "opencensus/context/context.h"
#include "opencensus/tags/tag_map.h"

using ::opencensus::context::Context;
using ::opencensus::tags::TagMap;

namespace opencensus {
namespace tags {

WithTagMap::WithTagMap(const TagMap& tags, bool cond)
    : swapped_tags_(tags)
#ifndef NDEBUG
      ,
      original_context_(Context::InternalMutableCurrent())
#endif
      ,
      cond_(cond) {
  ConditionalSwap();
}

WithTagMap::WithTagMap(TagMap&& tags, bool cond)
    : swapped_tags_(std::move(tags))
#ifndef NDEBUG
      ,
      original_context_(Context::InternalMutableCurrent())
#endif
      ,
      cond_(cond) {
  ConditionalSwap();
}

WithTagMap::~WithTagMap() { ConditionalSwap(); }

void WithTagMap::ConditionalSwap() {
  if (cond_) {
    using std::swap;
    swap(Context::InternalMutableCurrent()->tags_, swapped_tags_);
  }
}

}  // namespace tags
}  // namespace opencensus
