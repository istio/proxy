// Copyright 2024 Google LLC
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

class MyRootContext : public RootContext {
public:
  explicit MyRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}

  bool onStart(size_t) override {
#if defined(PROXY_WASM_PROTOBUF_FULL)
    LOG_TRACE("onStart with protobuf (full)");
    google::protobuf::Value value;
    value.set_string_value("unused");
#elif defined(PROXY_WASM_PROTOBUF_LITE)
    LOG_TRACE("onStart with protobuf (lite)");
    google::protobuf::Value value;
    value.set_string_value("unused");
#else
    LOG_TRACE("onStart without protobuf");
#endif
    return true;
  }
};

class MyHttpContext : public Context {
public:
  explicit MyHttpContext(uint32_t id, RootContext *root) : Context(id, root) {}
};

static RegisterContextFactory register_StaticContext(CONTEXT_FACTORY(MyHttpContext),
                                                     ROOT_FACTORY(MyRootContext));
