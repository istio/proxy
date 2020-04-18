/* Copyright 2020 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "absl/strings/str_join.h"
#include "extensions/attributegen/config.pb.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"
// Do not reorder.
#include "extensions/common/proxy_expr.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

using WasmResult = Envoy::Extensions::Common::Wasm::WasmResult;
using NullPluginRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRegistry;
using Envoy::Extensions::Common::Wasm::Null::Plugin::FilterStatus;

#include "api/wasm/cpp/contrib/proxy_expr.h"

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace AttributeGen {

using StringView = absl::string_view;

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

class Match {
 public:
  explicit Match(std::string condition, uint32_t condition_token,
                 std::string value)
      : condition_(condition),
        condition_token_(condition_token),
        value_(value){};

  // Returns the result of evaluation or nothing in case of an error.
  Optional<bool> evaluate() const {
    if (condition_.empty()) {
      LOG_DEBUG(absl::StrCat("Matched empty condition: value=", value_));
      return true;
    }

    bool matched;
    if (!evaluateExpression(condition_token_, &matched)) {
      LOG_WARN(absl::StrCat("Failed to evaluate expression: ", condition_));
      return {};
    }
    if (matched) {
      LOG_DEBUG(absl::StrCat("Matched: ", condition_));
    }
    return matched;
  };

  std::string value() const { return value_; };

  void cleanup() {
    if (!condition_.empty()) {
      exprDelete(condition_token_);
    }
  }

 protected:
  // Only used for debug messages after construction.
  std::string condition_;
  // Expression token associated with the condition.
  uint32_t condition_token_;
  std::string value_;
};

enum EvalPhase { on_log, on_request };

class AttributeGenerator {
 public:
  explicit AttributeGenerator(EvalPhase phase, std::string output_attribute)
      : phase_(phase),
        output_attribute_(output_attribute),
        error_attribute_(absl::StrCat(output_attribute, "_error")) {}

  // If evaluation is successful returns true and sets result.
  Optional<bool> evaluate(std::string* val) const {
    for (const auto& match : matches_) {
      auto eval_status = match.evaluate();
      if (!eval_status) {
        return {};
      }
      if (eval_status.value()) {
        *val = match.value();
        return true;
      }
    }
    return false;
  }

  EvalPhase phase() const { return phase_; }
  std::string output_attribute() const { return output_attribute_; }
  std::string error_attribute() const { return error_attribute_; }
  void add_match(Match match) { matches_.push_back(match); }
  void cleanup() {
    for (auto match : matches_) {
      match.cleanup();
    }
    matches_.clear();
  }

 protected:
  EvalPhase phase_;
  std::string output_attribute_;
  std::string error_attribute_;
  std::vector<Match> matches_;
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target
// for interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {}
  ~PluginRootContext() = default;

  bool onConfigure(size_t) override;
  // is called even if
  bool onDone() override {
    cleanupAttributeGen();
    return true;
  };
  void attributeGen(EvalPhase);

 protected:
  // Destroy host resources for the allocated expressions.
  void cleanupAttributeGen();

  bool initAttributeGen(const istio::attributegen::PluginConfig& config);

 private:
  std::vector<AttributeGenerator> gen_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onLog() override { rootContext()->attributeGen(on_log); };

  FilterHeadersStatus onRequestHeaders(uint32_t) override {
    rootContext()->attributeGen(on_request);
    return FilterHeadersStatus::Continue;
  }

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
};

#ifdef NULL_PLUGIN
NULL_PLUGIN_REGISTRY;
#endif

static RegisterContextFactory register_AttributeGen(
    CONTEXT_FACTORY(AttributeGen::PluginContext),
    ROOT_FACTORY(AttributeGen::PluginRootContext));

}  // namespace AttributeGen

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
