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
#include "envoy/server/factory_context.h"
#include "envoy/singleton/manager.h"
#include "envoy/thread_local/thread_local.h"
#include "source/common/common/non_copyable.h"
#include "source/common/config/subscription_base.h"
#include "source/common/grpc/common.h"
#include "source/common/init/target_impl.h"
#include "source/extensions/common/workload_discovery/discovery.pb.h"
#include "source/extensions/common/workload_discovery/discovery.pb.validate.h"
#include "source/extensions/common/workload_discovery/extension.pb.h"
#include "source/extensions/common/workload_discovery/extension.pb.validate.h"

namespace Envoy::Extensions::Common::WorkloadDiscovery {

namespace {
Istio::Common::WorkloadMetadataObject convert(const istio::workload::Workload& workload) {
  auto workload_type = Istio::Common::WorkloadType::Deployment;
  switch (workload.workload_type()) {
  case istio::workload::WorkloadType::CRONJOB:
    workload_type = Istio::Common::WorkloadType::CronJob;
    break;
  case istio::workload::WorkloadType::JOB:
    workload_type = Istio::Common::WorkloadType::Job;
    break;
  case istio::workload::WorkloadType::POD:
    workload_type = Istio::Common::WorkloadType::Pod;
    break;
  default:
    break;
  }
  return Istio::Common::WorkloadMetadataObject(
      workload.name(), workload.cluster_id(), workload.namespace_(), workload.workload_name(),
      workload.canonical_name(), workload.canonical_revision(), workload.canonical_name(),
      workload.canonical_revision(), workload_type);
}
} // namespace

class WorkloadMetadataProviderImpl : public WorkloadMetadataProvider, public Singleton::Instance {
public:
  WorkloadMetadataProviderImpl(const envoy::config::core::v3::ConfigSource& config_source,
                               Server::Configuration::ServerFactoryContext& factory_context)
      : config_source_(config_source), factory_context_(factory_context),
        tls_(factory_context.threadLocal()),
        scope_(factory_context.scope().createScope("workload_discovery")),
        stats_(generateStats(*scope_)), subscription_(*this) {
    tls_.set([](Event::Dispatcher&) { return std::make_shared<ThreadLocalProvider>(); });
    // This is safe because the ADS mux is started in the cluster manager constructor prior to this
    // call.
    subscription_.start();
  }

  std::optional<Istio::Common::WorkloadMetadataObject>
  GetMetadata(const Network::Address::InstanceConstSharedPtr& address) override {
    if (address && address->ip()) {
      if (const auto ipv4 = address->ip()->ipv4(); ipv4) {
        uint32_t value = ipv4->address();
        std::array<uint8_t, 4> output;
        absl::little_endian::Store32(&output, value);
        return tls_->get(std::string(output.begin(), output.end()));
      } else if (const auto ipv6 = address->ip()->ipv6(); ipv6) {
        const uint64_t high = absl::Uint128High64(ipv6->address());
        const uint64_t low = absl::Uint128Low64(ipv6->address());
        std::array<uint8_t, 16> output;
        absl::little_endian::Store64(&output, high);
        absl::little_endian::Store64(&output[8], low);
        return tls_->get(std::string(output.begin(), output.end()));
      }
    }
    return {};
  }

private:
  using AddressIndex = absl::flat_hash_map<std::string, Istio::Common::WorkloadMetadataObject>;
  using AddressIndexSharedPtr = std::shared_ptr<AddressIndex>;
  using AddressVector = std::vector<std::string>;
  using AddressVectorSharedPtr = std::shared_ptr<AddressVector>;

  struct ThreadLocalProvider : public ThreadLocal::ThreadLocalObject {
    void reset(const AddressIndexSharedPtr& index) { address_index_ = *index; }
    void update(const AddressIndexSharedPtr& added, const AddressVectorSharedPtr& removed) {
      for (const auto& [address, workload] : *added) {
        address_index_.emplace(address, workload);
      }
      for (const auto& address : *removed) {
        address_index_.erase(address);
      }
    }
    size_t total() const { return address_index_.size(); }
    // Returns by-value since the flat map does not provide pointer stability.
    std::optional<Istio::Common::WorkloadMetadataObject> get(const std::string& address) {
      const auto it = address_index_.find(address);
      if (it != address_index_.end()) {
        return it->second;
      }
      return {};
    }
    AddressIndex address_index_;
  };
  class WorkloadSubscription : Config::SubscriptionBase<istio::workload::Workload> {
  public:
    WorkloadSubscription(WorkloadMetadataProviderImpl& parent)
        : Config::SubscriptionBase<istio::workload::Workload>(
              parent.factory_context_.messageValidationVisitor(), "uid"),
          parent_(parent) {
      subscription_ = parent.factory_context_.clusterManager()
                          .subscriptionFactory()
                          .subscriptionFromConfigSource(
                              parent.config_source_, Grpc::Common::typeUrl(getResourceName()),
                              *parent.scope_, *this, resource_decoder_, {});
    }
    void start() { subscription_->start({}); }

  private:
    // Config::SubscriptionCallbacks
    absl::Status onConfigUpdate(const std::vector<Config::DecodedResourceRef>& resources,
                                const std::string&) override {
      AddressIndexSharedPtr index = std::make_shared<AddressIndex>();
      for (const auto& resource : resources) {
        const auto& workload =
            dynamic_cast<const istio::workload::Workload&>(resource.get().resource());
        const auto& metadata = convert(workload);
        for (const auto& addr : workload.addresses()) {
          index->emplace(addr, metadata);
        }
      }
      parent_.reset(index);
      return absl::OkStatus();
    }
    // TODO(kuat) This is not working correctly due to breakage by "uid" PR.
    absl::Status onConfigUpdate(const std::vector<Config::DecodedResourceRef>& added_resources,
                                const Protobuf::RepeatedPtrField<std::string>& removed_resources,
                                const std::string&) override {
      AddressIndexSharedPtr added = std::make_shared<AddressIndex>();
      for (const auto& resource : added_resources) {
        const auto& workload =
            dynamic_cast<const istio::workload::Workload&>(resource.get().resource());
        const auto& metadata = convert(workload);
        added->emplace(workload.uid(), metadata);
        for (const auto& addr : workload.addresses()) {
          added->emplace(addr, metadata);
        }
      }
      AddressVectorSharedPtr removed = std::make_shared<AddressVector>();
      removed->reserve(removed_resources.size());
      for (const auto& resource : removed_resources) {
        removed->push_back(resource);
      }
      parent_.update(added, removed);
      return absl::OkStatus();
    }
    void onConfigUpdateFailed(Config::ConfigUpdateFailureReason, const EnvoyException*) override {
      // Do nothing - feature is automatically disabled.
      // TODO: Potential issue with the expiration of the metadata.
    }
    WorkloadMetadataProviderImpl& parent_;
    Config::SubscriptionPtr subscription_;
  };

  void reset(AddressIndexSharedPtr index) {
    tls_.runOnAllThreads([index](OptRef<ThreadLocalProvider> tls) { tls->reset(index); });
    stats_.total_.set(tls_->total());
  }

  void update(AddressIndexSharedPtr added, AddressVectorSharedPtr removed) {
    tls_.runOnAllThreads(
        [added, removed](OptRef<ThreadLocalProvider> tls) { tls->update(added, removed); });
    stats_.total_.set(tls_->total());
  }

  WorkloadDiscoveryStats generateStats(Stats::Scope& scope) {
    return WorkloadDiscoveryStats{WORKLOAD_DISCOVERY_STATS(POOL_GAUGE(scope))};
  }

  const envoy::config::core::v3::ConfigSource config_source_;
  Server::Configuration::ServerFactoryContext& factory_context_;
  ThreadLocal::TypedSlot<ThreadLocalProvider> tls_;
  Stats::ScopeSharedPtr scope_;
  WorkloadDiscoveryStats stats_;
  WorkloadSubscription subscription_;
};

SINGLETON_MANAGER_REGISTRATION(workload_metadata_provider)

class WorkloadDiscoveryExtension : public Server::BootstrapExtension {
public:
  WorkloadDiscoveryExtension(Server::Configuration::ServerFactoryContext& factory_context,
                             const istio::workload::BootstrapExtension& config)
      : factory_context_(factory_context), config_(config) {}

  // Server::Configuration::BootstrapExtension
  void onServerInitialized() override {
    provider_ = factory_context_.singletonManager().getTyped<WorkloadMetadataProvider>(
        SINGLETON_MANAGER_REGISTERED_NAME(workload_metadata_provider), [&] {
          return std::make_shared<WorkloadMetadataProviderImpl>(config_.config_source(),
                                                                factory_context_);
        });
  }

private:
  Server::Configuration::ServerFactoryContext& factory_context_;
  const istio::workload::BootstrapExtension config_;
  WorkloadMetadataProviderSharedPtr provider_;
};

class WorkloadDiscoveryFactory : public Server::Configuration::BootstrapExtensionFactory {
public:
  // Server::Configuration::BootstrapExtensionFactory
  Server::BootstrapExtensionPtr
  createBootstrapExtension(const Protobuf::Message& config,
                           Server::Configuration::ServerFactoryContext& context) override {
    const auto& message =
        MessageUtil::downcastAndValidate<const istio::workload::BootstrapExtension&>(
            config, context.messageValidationVisitor());
    return std::make_unique<WorkloadDiscoveryExtension>(context, message);
  }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<istio::workload::BootstrapExtension>();
  }
  std::string name() const override { return "envoy.bootstrap.workload_discovery"; };
};

REGISTER_FACTORY(WorkloadDiscoveryFactory, Server::Configuration::BootstrapExtensionFactory);

WorkloadMetadataProviderSharedPtr
GetProvider(Server::Configuration::ServerFactoryContext& context) {
  return context.singletonManager().getTyped<WorkloadMetadataProvider>(
      SINGLETON_MANAGER_REGISTERED_NAME(workload_metadata_provider));
}

} // namespace Envoy::Extensions::Common::WorkloadDiscovery
