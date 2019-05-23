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

#include "opencensus/context/context.h"

#include <functional>
#include <utility>

#include "absl/strings/str_cat.h"
#include "opencensus/context/with_context.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace context {

Context::Context() : tags_(opencensus::tags::TagMap({})) {}

// static
const Context& Context::Current() { return *InternalMutableCurrent(); }

std::function<void()> Context::Wrap(std::function<void()> fn) const {
  Context copy(Context::Current());
  return [fn, copy]() {
    WithContext wc(copy);
    fn();
  };
}

std::string Context::DebugString() const {
  return absl::StrCat("ctx@", absl::Hex(this),
                      " tags=", tags_.DebugString());
}

// static
Context* Context::InternalMutableCurrent() {
  static Context* thread_ctx = nullptr;
  if (thread_ctx == nullptr) thread_ctx = new Context;
  return thread_ctx;
}

void swap(Context& a, Context& b) {
  using std::swap;
  swap(a.tags_, b.tags_);
}

}  // namespace context
}  // namespace opencensus
