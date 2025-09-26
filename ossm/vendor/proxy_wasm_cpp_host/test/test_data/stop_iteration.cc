// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "proxy_wasm_intrinsics.h"

class StopIterationContext : public Context {
public:
  explicit StopIterationContext(uint32_t id, RootContext *root) : Context(id, root) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override {
    return FilterHeadersStatus::StopIteration;
  }

  FilterHeadersStatus onResponseHeaders(uint32_t headers, bool end_of_stream) override {
    return FilterHeadersStatus::StopIteration;
  }
};

static RegisterContextFactory register_StaticContext(CONTEXT_FACTORY(StopIterationContext),
                                                     ROOT_FACTORY(RootContext));
