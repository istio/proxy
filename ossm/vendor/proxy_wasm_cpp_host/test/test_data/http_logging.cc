// Copyright 2023 Google LLC
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

class LoggingContext : public Context {
public:
  explicit LoggingContext(uint32_t id, RootContext *root) : Context(id, root) {}

  void onCreate() override { LOG_INFO("onCreate called"); }
  void onDelete() override { LOG_INFO("onDelete called"); }
  void onDone() override { LOG_INFO("onDone called"); }

  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override {
    LOG_INFO("onRequestHeaders called");
    return FilterHeadersStatus::Continue;
  }

  FilterHeadersStatus onResponseHeaders(uint32_t headers, bool end_of_stream) override {
    LOG_INFO("onResponseHeaders called");
    return FilterHeadersStatus::Continue;
  }
};

static RegisterContextFactory register_StaticContext(CONTEXT_FACTORY(LoggingContext),
                                                     ROOT_FACTORY(RootContext));
