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

#include "opencensus/context/with_context.h"

#include <utility>

#include "opencensus/context/context.h"

namespace opencensus {
namespace context {

WithContext::WithContext(const Context& ctx, bool cond)
    : swapped_context_(cond ? ctx : Context())
#ifndef NDEBUG
      ,
      original_context_(Context::InternalMutableCurrent())
#endif
      ,
      cond_(cond) {
  ConditionalSwap();
}

WithContext::WithContext(Context&& ctx, bool cond)
    : swapped_context_(cond ? std::move(ctx) : Context())
#ifndef NDEBUG
      ,
      original_context_(Context::InternalMutableCurrent())
#endif
      ,
      cond_(cond) {
  ConditionalSwap();
}

WithContext::~WithContext() { ConditionalSwap(); }

void WithContext::ConditionalSwap() {
  if (cond_) {
    using std::swap;
    swap(*Context::InternalMutableCurrent(), swapped_context_);
  }
}

}  // namespace context
}  // namespace opencensus
