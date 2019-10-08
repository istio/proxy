/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "extensions/common/wasm/null/null.h"
#include "stackdriver.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
namespace Stackdriver {
NullPluginRootRegistry* context_registry_{};
}  // namespace Stackdriver

constexpr char kStackdriverPluginName[] = "envoy.wasm.null.stackdriver";

/**
 * Config registration for a Wasm filter plugin. @see
 * NamedHttpFilterConfigFactory.
 */
class StackdriverPluginFactory : public NullVmPluginFactory {
 public:
  StackdriverPluginFactory() {}

  const std::string name() const override { return kStackdriverPluginName; }
  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullPlugin>(
        Envoy::Extensions::Common::Wasm::Null::Plugin::Stackdriver::
            context_registry_);
  }
};

/**
 * Static registration for the null Wasm filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<StackdriverPluginFactory, NullVmPluginFactory>
    register_;

}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
