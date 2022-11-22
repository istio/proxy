// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/extensions/common/workload_discovery/api.h"

#include "envoy/registry/registry.h"
#include "envoy/server/bootstrap_extension_config.h"

namespace Envoy::Extensions::Common::WorkloadDiscovery {

SINGLETON_MANAGER_REGISTRATION(WorkloadMetadataProvider)

class WorkloadDiscoveryExtension : public Server::BootstrapExtension {
 public:
  WorkloadDiscoveryExtension(
      Server::Configuration::ServerFactoryContext& factory_context,
      const istio::workload_discovery::v1::BootstrapExtension& config)
      : factory_context_(factory_context), config_(config) {}

  // Server::Configuration::BootstrapExtension
  void onServerInitialized() override {
    provider_ =
        factory_context_.singletonManager().getTyped<WorkloadMetadataProvider>(
            SINGLETON_MANAGER_REGISTERED_NAME(WorkloadMetadataProvider), [&] {
              return std::make_shared<WorkloadMetadataProvider>(
                  config_.config_source(), factory_context_);
            });
    /* Example:
    provider_->fetch("127.0.0.1", [](const WorkloadRecordSharedPtr& record) {
      std::cout << "1: " << (record->metadata_.has_value() ?
    record->metadata_->baggage() : "(none)")
                << std::endl;
    });
    provider_->fetch("127.0.0.2", [](const WorkloadRecordSharedPtr& record) {
      std::cout << "2: " << (record->metadata_.has_value() ?
    record->metadata_->baggage() : "(none)")
                << std::endl;
    });
    */
  }

 private:
  Server::Configuration::ServerFactoryContext& factory_context_;
  const istio::workload_discovery::v1::BootstrapExtension config_;
  WorkloadMetadataProviderSharedPtr provider_;
};

class WorkloadDiscoveryFactory
    : public Server::Configuration::BootstrapExtensionFactory {
 public:
  // Server::Configuration::BootstrapExtensionFactory
  Server::BootstrapExtensionPtr createBootstrapExtension(
      const Protobuf::Message& config,
      Server::Configuration::ServerFactoryContext& context) override {
    const auto& message = MessageUtil::downcastAndValidate<
        const istio::workload_discovery::v1::BootstrapExtension&>(
        config, context.messageValidationVisitor());
    return std::make_unique<WorkloadDiscoveryExtension>(context, message);
  }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<
        istio::workload_discovery::v1::BootstrapExtension>();
  }
  std::string name() const override {
    return "envoy.bootstrap.workload_discovery";
  };
};

REGISTER_FACTORY(WorkloadDiscoveryFactory,
                 Server::Configuration::BootstrapExtensionFactory);

}  // namespace Envoy::Extensions::Common::WorkloadDiscovery
