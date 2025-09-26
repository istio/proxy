// Copyright 2021 Google LLC
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

#include <string>
#include <string_view>
#include <unordered_map>

#include "proxy_wasm_intrinsics.h"

class CanaryCheckRootContext1 : public RootContext {
public:
  explicit CanaryCheckRootContext1(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id) {}
  bool onConfigure(size_t s) override {
    LOG_TRACE("onConfigure: root_id_1");
    return s != 0;
  }
};

class CanaryCheckContext : public Context {
public:
  explicit CanaryCheckContext(uint32_t id, RootContext *root) : Context(id, root) {}
};

class CanaryCheckRootContext2 : public RootContext {
public:
  explicit CanaryCheckRootContext2(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id) {}
  bool onConfigure(size_t s) override {
    LOG_TRACE("onConfigure: root_id_2");
    return s != 0;
  }
};

static RegisterContextFactory register_CanaryCheckContext1(CONTEXT_FACTORY(CanaryCheckContext),
                                                           ROOT_FACTORY(CanaryCheckRootContext1),
                                                           "root_id_1");

static RegisterContextFactory register_CanaryCheckContext2(CONTEXT_FACTORY(CanaryCheckContext),
                                                           ROOT_FACTORY(CanaryCheckRootContext2),
                                                           "root_id_2");
