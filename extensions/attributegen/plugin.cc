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

#include "extensions/attributegen/plugin.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace AttributeGen {

// class Match
// Returns the result of evaluation or nothing in case of an error.
Optional<bool> Match::evaluate() const {
  if (condition_.empty()) {
    return true;
  }

  const std::string function = "expr_evaluate";
  char* out = nullptr;
  size_t out_size = 0;
  auto result = proxy_call_foreign_function(
      function.data(), function.size(),
      reinterpret_cast<const char*>(&condition_token_), sizeof(uint32_t), &out,
      &out_size);
  if (result != WasmResult::Ok) {
    LOG_TRACE(absl::StrCat("Failed to evaluate expression:[", condition_token_,
                           "] ", condition_, " result: ", toString(result)));
    return {};
  }
  if (out_size != sizeof(bool)) {
    LOG_TRACE(absl::StrCat("Expression:[", condition_token_, "] ", condition_,
                           " did not return a bool, size:", out_size));
    free(out);
    return {};
  }

  bool matched = *reinterpret_cast<bool*>(out);

  free(out);
  return matched;
}

// end class Match

// class AttributeGenerator

// If evaluation is successful returns true and sets result.
Optional<bool> AttributeGenerator::evaluate(std::string* val) const {
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

// end class AttributeGenerator

bool PluginRootContext::onConfigure(size_t) {
  std::unique_ptr<WasmData> configuration = getConfiguration();
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  istio::attributegen::PluginConfig config;
  Status status =
      JsonStringToMessage(configuration->toString(), &config, json_options);
  if (status != Status::OK) {
    LOG_WARN(absl::StrCat(
        "Cannot parse 'attributegen' plugin configuration JSON string [YAML is "
        "not supported]: ",
        configuration->toString()));
    incrementMetric(config_errors_, 1);
    return false;
  }

  debug_ = config.debug();

  cleanupAttributeGen();
  auto init_status = initAttributeGen(config);
  if (!init_status) {
    incrementMetric(config_errors_, 1);
    cleanupAttributeGen();
  }

  return init_status;
}

bool PluginRootContext::initAttributeGen(
    const istio::attributegen::PluginConfig& config) {
  for (const auto& attribute_gen_config : config.attributes()) {
    EvalPhase phase = OnLog;
    if (attribute_gen_config.phase() == istio::attributegen::ON_REQUEST) {
      phase = OnRequest;
    }
    std::vector<Match> matches;

    for (const auto& matchconfig : attribute_gen_config.match()) {
      uint32_t token = 0;
      if (matchconfig.condition().empty()) {
        matches.push_back(Match("", 0, matchconfig.value()));
        continue;
      }
      auto create_status = createExpression(matchconfig.condition(), &token);

      if (create_status != WasmResult::Ok) {
        LOG_WARN(absl::StrCat("Cannot create expression: <",
                              matchconfig.condition(), "> for ",
                              attribute_gen_config.output_attribute(),
                              " result:", toString(create_status)));
        return false;
      }
      if (debug_) {
        LOG_DEBUG(absl::StrCat(
            "Added [", token, "] ", attribute_gen_config.output_attribute(),
            " if (", matchconfig.condition(), ") -> ", matchconfig.value()));
      }

      tokens_.push_back(token);
      matches.push_back(
          Match(matchconfig.condition(), token, matchconfig.value()));
    }
    gen_.push_back(AttributeGenerator(
        phase, attribute_gen_config.output_attribute(), std::move(matches)));
    matches.clear();
  }
  return true;
}

void PluginRootContext::cleanupAttributeGen() {
  gen_.clear();
  for (const auto& token : tokens_) {
    exprDelete(token);
  }
  tokens_.clear();
}

bool PluginRootContext::onDone() {
  cleanupAttributeGen();
  return true;
}

// attributeGen is called on the data path.
void PluginRootContext::attributeGen(EvalPhase phase) {
  for (const auto& attribute_generator : gen_) {
    if (phase != attribute_generator.phase()) {
      continue;
    }

    std::string val;
    auto eval_status = attribute_generator.evaluate(&val);
    if (!eval_status) {
      incrementMetric(runtime_errors_, 1);

      // eval failed set error attribute
      // __error attribute should be used by downstream plugins to distinguish
      // between
      // 1. No conditions matched.
      // 2. Error evaluating a condition.
      setFilterState(attribute_generator.errorAttribute(), "1");
      continue;
    }

    if (!eval_status.value()) {
      continue;
    }

    if (debug_) {
      LOG_DEBUG(absl::StrCat("Setting ", attribute_generator.outputAttribute(),
                             " --> ", val));
    }
    setFilterState(attribute_generator.outputAttribute(), val);
  }
}

#ifdef NULL_PLUGIN
NullPluginRegistry* context_registry_{};

class AttributeGenFactory : public NullVmPluginFactory {
 public:
  std::string name() const override { return "envoy.wasm.attributegen"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullPlugin>(context_registry_);
  }
};

static Registry::RegisterFactory<AttributeGenFactory, NullVmPluginFactory>
    register_;
#endif

}  // namespace AttributeGen

#ifdef NULL_PLUGIN
// WASM_EPILOG
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
