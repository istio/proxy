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

#include "common/common/base64.h"
#include "extensions/metadata_exchange/plugin.h"

namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace MetadataExchange {
namespace Plugin {
::Envoy::Extensions::Common::Wasm::Null::NullPluginRootRegistry*
    context_registry_{};
}  // namespace Plugin

// Registration glue

class MetadataExchangeFactory
    : public ::Envoy::Extensions::Common::Wasm::Null::NullVmPluginFactory {
 public:
  const std::string name() const override {
    return "envoy.wasm.metadata_exchange";
  }
  std::unique_ptr<::Envoy::Extensions::Common::Wasm::Null::NullVmPlugin>
  create() const override {
    return std::make_unique<
        ::Envoy::Extensions::Common::Wasm::Null::NullPlugin>(
        Plugin::context_registry_);
  }
};

static Registry::RegisterFactory<
    MetadataExchangeFactory,
    ::Envoy::Extensions::Common::Wasm::Null::NullVmPluginFactory>
    register_;

}  // namespace MetadataExchange
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
