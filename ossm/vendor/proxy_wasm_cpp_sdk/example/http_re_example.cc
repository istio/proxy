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
#include "re2/re2.h"

class MyRootContext : public RootContext {
public:
  explicit MyRootContext(uint32_t id, std::string_view root_id) : RootContext(id, root_id) {}

  bool onConfigure(size_t) override {
    // Compile the regex expression at plugin setup time.
    path_match.emplace("/foo-([^/]+)/");
    return path_match->ok();
  }

  std::optional<re2::RE2> path_match;
};

class MyHttpContext : public Context {
public:
  explicit MyHttpContext(uint32_t id, RootContext *root)
      : Context(id, root), root_(static_cast<MyRootContext *>(root)) {}

  FilterHeadersStatus onRequestHeaders(uint32_t headers, bool end_of_stream) override {
    auto path = getRequestHeader(":path");
    if (path) {
      std::string edit = path->toString(); // mutable copy
      if (re2::RE2::Replace(&edit, *root_->path_match, "/\\1/")) {
        replaceRequestHeader(":path", edit);
      }
    }
    return FilterHeadersStatus::Continue;
  }

private:
  const MyRootContext *root_;
};

static RegisterContextFactory register_StaticContext(CONTEXT_FACTORY(MyHttpContext),
                                                     ROOT_FACTORY(MyRootContext));
