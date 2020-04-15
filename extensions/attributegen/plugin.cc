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

#include "absl/strings/ascii.h"

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

bool PluginRootContext::onConfigure(size_t) {
  std::unique_ptr<WasmData> configuration = getConfiguration();
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  istio::attributegen::PluginConfig config;
  Status status =
      JsonStringToMessage(configuration->toString(), &config, json_options);
  if (status != Status::OK) {
    LOG_WARN(absl::StrCat("Cannot parse plugin configuration JSON string ",
                          configuration->toString()));
    return false;
  }

  return initAttributeGen(config);
}

bool PluginRootContext::initAttributeGen(
    const istio::attributegen::PluginConfig& config) {
  // Clear prior host resources.
  // TODO(mjog) see if this still needed since onDone is always called.
  cleanupAttributeGen();

  for (auto agconfig : config.attributes()) {
    EvalPhase phase = on_log;
    if (agconfig.phase() == istio::attributegen::ON_REQUEST) {
      phase = on_request;
    }
    auto ag = AttributeGenerator(phase, agconfig.output_attribute());
    gen_.push_back(ag);
    for (auto matchconfig : agconfig.match()) {
      uint32_t token = 0;
      if (createExpression(matchconfig.condition(), &token) != WasmResult::Ok) {
        LOG_WARN(
            absl::StrCat("Cannot create expression: ", matchconfig.condition(),
                         " for ", agconfig.output_attribute()));
        return false;
      }
      LOG_DEBUG(absl::StrCat("Added ", agconfig.output_attribute(), " if (",
                             matchconfig.condition(), ") -> ",
                             matchconfig.value()));
      ag.add_match(Match(matchconfig.condition(), token, matchconfig.value()));
    }
  }
  return true;
}

void PluginRootContext::cleanupAttributeGen() {
  for (auto ag : gen_) {
    ag.cleanup();
  }
  gen_.clear();
}

// attributeGen is called on the data path.
void PluginRootContext::attributeGen(EvalPhase phase) {
  for (const auto& ag : gen_) {
    if (phase != ag.phase()) {
      continue;
    }

    std::string val;
    auto eval_status = ag.evaluate(&val);
    if (!eval_status) {
      // eval failed set error attribute
      setFilterState(ag.error_attribute(), "1");
      continue;
    }

    if (!eval_status.value()) {
      continue;
    }

    setFilterState(ag.output_attribute(), val);
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
