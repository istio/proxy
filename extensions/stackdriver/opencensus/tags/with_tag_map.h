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

#ifndef OPENCENSUS_TAGS_WITH_TAG_MAP_H_
#define OPENCENSUS_TAGS_WITH_TAG_MAP_H_

#include "opencensus/context/context.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace tags {

// WithTagMap is a scoped object that sets the current TagMap to the given one,
// until the WithTagMap object is destroyed. If the condition is false, it
// doesn't do anything.
//
// Because WithTagMap changes the current (thread local) context, NEVER allocate
// a WithTagMap in one thread and deallocate in another. A simple way to ensure
// this is to only ever stack-allocate it.
//
// Example usage:
// {
//   WithTagMap wt(tags);
//   // Do work.
// }
class WithTagMap {
 public:
  explicit WithTagMap(const TagMap& tags, bool cond = true);
  explicit WithTagMap(TagMap&& tags, bool cond = true);
  ~WithTagMap();

 private:
  WithTagMap() = delete;
  WithTagMap(const WithTagMap&) = delete;
  WithTagMap(WithTagMap&&) = delete;
  WithTagMap& operator=(const WithTagMap&) = delete;
  WithTagMap& operator=(WithTagMap&&) = delete;

  void ConditionalSwap();

  TagMap swapped_tags_;
#ifndef NDEBUG
  const ::opencensus::context::Context* original_context_;
#endif
  const bool cond_;
};

}  // namespace tags
}  // namespace opencensus

#endif  // OPENCENSUS_TAGS_WITH_TAG_MAP_H_
