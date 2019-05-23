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

#ifndef OPENCENSUS_TAGS_CONTEXT_UTIL_H_
#define OPENCENSUS_TAGS_CONTEXT_UTIL_H_

#include "opencensus/context/context.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace tags {

// Returns the TagMap from the current Context.
const TagMap& GetCurrentTagMap();

// Returns the TagMap from the given Context.
const TagMap& GetTagMapFromContext(const opencensus::context::Context& ctx);

}  // namespace tags
}  // namespace opencensus

#endif  // OPENCENSUS_TAGS_CONTEXT_UTIL_H_
