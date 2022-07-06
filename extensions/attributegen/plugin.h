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

#include <optional>

#include "absl/strings/str_join.h"
#include "extensions/attributegen/config.pb.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"
// Do not reorder.
#include "contrib/proxy_expr.h"

#else  // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {

#include "contrib/proxy_expr.h"

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace AttributeGen {

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

class Match {
 public:
  explicit Match(const std::string& condition, uint32_t condition_token,
                 const std::string& value)
      : condition_(condition),
        condition_token_(condition_token),
        value_(value){};

  std::optional<bool> evaluate() const;
  const std::string& value() const { return value_; };

 private:
  const std::string condition_;
  // Expression token associated with the condition.
  const uint32_t condition_token_;
  const std::string value_;
};

enum EvalPhase { OnLog, OnRequest };

class AttributeGenerator {
 public:
  explicit AttributeGenerator(EvalPhase phase,
                              const std::string& output_attribute,
                              const std::vector<Match>& matches)
      : phase_(phase),
        output_attribute_(output_attribute),
        matches_(std::move(matches)) {}

  // If evaluation is successful returns true and sets result.
  std::optional<bool> evaluate(std::string* val) const;
  EvalPhase phase() const { return phase_; }
  const std::string& outputAttribute() const { return output_attribute_; }

 private:
  EvalPhase phase_;
  const std::string output_attribute_;
  const std::vector<Match> matches_;
};

// PluginRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target
// for interactions that outlives individual stream, e.g. timer, async calls.
class PluginRootContext : public RootContext {
 public:
  PluginRootContext(uint32_t id, std::string_view root_id)
      : RootContext(id, root_id) {
    Metric error_count(MetricType::Counter, "error_count",
                       {MetricTag{"wasm_filter", MetricTag::TagType::String},
                        MetricTag{"type", MetricTag::TagType::String}});
    config_errors_ = error_count.resolve("attributegen", "config");
    runtime_errors_ = error_count.resolve("attributegen", "runtime");
  }

  bool onConfigure(size_t) override;
  bool onDone() override;
  void attributeGen(EvalPhase);

 private:
  // Destroy host resources for the allocated expressions.
  void cleanupAttributeGen();
  bool initAttributeGen(const istio::attributegen::PluginConfig& config);

  // list of generators.
  std::vector<AttributeGenerator> gen_;
  // Token are created and destroyed by PluginContext.
  std::vector<uint32_t> tokens_;

  bool debug_;

  // error counter metrics.
  uint32_t config_errors_;
  uint32_t runtime_errors_;
};

// Per-stream context.
class PluginContext : public Context {
 public:
  explicit PluginContext(uint32_t id, RootContext* root) : Context(id, root) {}

  void onLog() override { rootContext()->attributeGen(OnLog); };

  FilterHeadersStatus onRequestHeaders(uint32_t, bool) override {
    rootContext()->attributeGen(OnRequest);
    return FilterHeadersStatus::Continue;
  }

 private:
  inline PluginRootContext* rootContext() {
    return dynamic_cast<PluginRootContext*>(this->root());
  };
};

#ifdef NULL_PLUGIN
PROXY_WASM_NULL_PLUGIN_REGISTRY;
#endif

static RegisterContextFactory register_AttributeGen(
    CONTEXT_FACTORY(AttributeGen::PluginContext),
    ROOT_FACTORY(AttributeGen::PluginRootContext));

}  // namespace AttributeGen

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
