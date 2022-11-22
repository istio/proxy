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

#pragma once

#include "envoy/server/factory_context.h"
#include "envoy/singleton/manager.h"
#include "envoy/thread_local/thread_local.h"
#include "extensions/common/metadata_object.h"
#include "source/common/common/non_copyable.h"
#include "source/common/config/subscription_base.h"
#include "source/common/grpc/common.h"
#include "source/common/init/target_impl.h"
#include "source/extensions/common/workload_discovery/discovery.pb.h"
#include "source/extensions/common/workload_discovery/discovery.pb.validate.h"

namespace Envoy::Extensions::Common::WorkloadDiscovery {

/** Metadata discovery API client maintains subscriptions to "known" IPs for
 * the purpose of a policy and telemetry cache by IPs. The algorithm works as
 * follows:
 *
 * 1. Each worker maintains a thread local index to shared entries.
 *
 * 2. If a worker cannot locate an entry, it issues a request to subscribe and
 * a callback.
 *
 * 3. If the main receives a record from xDS, it invokes callbacks and posts
 * updates to all workers.
 *
 * 4. The main is responsible for the cache expiration. It can post a deletion
 * to worker.
 */

struct WorkloadRecord : NonCopyable {
  WorkloadRecord(absl::optional<Istio::Common::WorkloadMetadataObject> metadata)
      : metadata_(metadata) {}

  // A record may indicate an "absent metadata" which are also cached.
  const absl::optional<Istio::Common::WorkloadMetadataObject> metadata_;
};

using WorkloadRecordSharedPtr = std::shared_ptr<WorkloadRecord>;
using WorkloadMetadataResource =
    istio::workload_discovery::v1::WorkloadMetadataResource;
using WorkloadCb = std::function<void(const WorkloadRecordSharedPtr&)>;

namespace {
WorkloadRecordSharedPtr fromProto(const WorkloadMetadataResource& proto) {
  return std::make_shared<WorkloadRecord>(
      absl::make_optional<Istio::Common::WorkloadMetadataObject>(
          proto.instance_name(), "" /* cluster_name */, proto.namespace_name(),
          proto.workload_name(), proto.canonical_name(),
          proto.canonical_revision(), "" /* app_name */, "" /* app_version */,
          Istio::Common::WorkloadType::Pod));
}
}  // namespace

// Singleton asynchronous xDS provider of IP metadata with thread-local caches.
class WorkloadMetadataProvider
    : public Singleton::Instance,
      public std::enable_shared_from_this<WorkloadMetadataProvider> {
 public:
  WorkloadMetadataProvider(
      const envoy::config::core::v3::ConfigSource& config_source,
      Server::Configuration::ServerFactoryContext& factory_context)
      : config_source_(config_source),
        factory_context_(factory_context),
        tls_(factory_context.threadLocal()),
        scope_(factory_context.scope().createScope("workload_discovery")) {
    tls_.set([](Event::Dispatcher&) {
      return std::make_shared<ThreadLocalProvider>();
    });
  }

  // Asynchronous metadata fetcher (called from workers).
  void fetch(const std::string& ip, WorkloadCb completion) {
    bool fetched = tls_->fetch(ip, completion);
    if (!fetched) {
      factory_context_.mainThreadDispatcher().post(
          [ip, me = std::weak_ptr<WorkloadMetadataProvider>(
                   shared_from_this())]() -> void {
            if (auto self = me.lock(); self) {
              self->subscribe(ip);
            }
          });
    }
  }

 private:
  struct ThreadLocalProvider : public ThreadLocal::ThreadLocalObject {
    void update(const std::string& ip, const WorkloadRecordSharedPtr& record) {
      ip_index_[ip] = record;
      const auto& it = completions_.find(ip);
      if (it != completions_.end()) {
        for (const auto& cb : it->second) {
          cb(record);
        }
        completions_.erase(it);
      }
    }
    bool fetch(const std::string& ip, WorkloadCb completion) {
      const auto& it = ip_index_.find(ip);
      if (it != ip_index_.end()) {
        completion(it->second);
        return true;
      }
      completions_[ip].push_back(completion);
      return false;
    }
    // Assuming short-string optimization in clang, 15 characters for IPv4 are
    // inlined.
    // XXX: This will probably evolve to a CIDR matcher.
    absl::flat_hash_map<std::string, WorkloadRecordSharedPtr> ip_index_;
    absl::flat_hash_map<std::string, std::vector<WorkloadCb>> completions_;
  };
  class WorkloadSubscription
      : Config::SubscriptionBase<WorkloadMetadataResource> {
   public:
    WorkloadSubscription(WorkloadMetadataProvider& parent,
                         const std::string& name)
        : Config::SubscriptionBase<WorkloadMetadataResource>(
              parent.factory_context_.messageValidationVisitor(), "name"),
          parent_(parent),
          name_(name) {
      subscription_ = parent.factory_context_.clusterManager()
                          .subscriptionFactory()
                          .subscriptionFromConfigSource(
                              parent.config_source_,
                              Grpc::Common::typeUrl(getResourceName()),
                              *parent.scope_, *this, resource_decoder_, {});
      subscription_->start({name_});
      std::cout << "SUBSCRIBING TO " << name_ << std::endl;
    }
    // TODO: subscription cancel

   private:
    // Config::SubscriptionCallbacks
    void onConfigUpdate(const std::vector<Config::DecodedResourceRef>&,
                        const std::string&) override {
      ASSERT(false);
    }
    void onConfigUpdate(
        const std::vector<Config::DecodedResourceRef>& added_resources,
        const Protobuf::RepeatedPtrField<std::string>& removed_resources,
        const std::string&) override {
      if (!removed_resources.empty()) {
        ASSERT(removed_resources.size() == 1,
               "Must be single resource deleted");
        parent_.update(name_, std::make_shared<WorkloadRecord>(absl::nullopt));
      } else if (!added_resources.empty()) {
        ASSERT(added_resources.size() == 1, "Must be single resource added");
        const auto& proto = dynamic_cast<const WorkloadMetadataResource&>(
            added_resources[0].get().resource());
        parent_.update(name_, fromProto(proto));
      } else {
        ASSERT(false, "Unexpected update");
      }
    }
    void onConfigUpdateFailed(Config::ConfigUpdateFailureReason,
                              const EnvoyException*) override {
      // Ensures that completions are invoked.
      parent_.update(name_, std::make_shared<WorkloadRecord>(absl::nullopt));
    }
    WorkloadMetadataProvider& parent_;
    const std::string name_;
    Config::SubscriptionPtr subscription_;
  };
  void subscribe(const std::string& ip) {
    TRY_ASSERT_MAIN_THREAD { subscriptions_.try_emplace(ip, *this, ip); }
    END_TRY
    catch (const EnvoyException& e) {
      ENVOY_LOG_MISC(warn, absl::StrCat("Failed to subscribe: ", e.what()));
    }
  }
  void update(const std::string& ip, WorkloadRecordSharedPtr record) {
    tls_.runOnAllThreads([ip, record](OptRef<ThreadLocalProvider> tls) {
      tls->update(ip, record);
    });
  }
  const envoy::config::core::v3::ConfigSource config_source_;
  Server::Configuration::ServerFactoryContext& factory_context_;
  ThreadLocal::TypedSlot<ThreadLocalProvider> tls_;
  Stats::ScopeSharedPtr scope_;
  // TODO: expiry
  absl::node_hash_map<std::string, WorkloadSubscription> subscriptions_;
};

using WorkloadMetadataProviderSharedPtr =
    std::shared_ptr<WorkloadMetadataProvider>;

}  // namespace Envoy::Extensions::Common::WorkloadDiscovery
