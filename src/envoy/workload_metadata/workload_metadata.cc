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

#include "workload_metadata.h"

#include "envoy/network/listen_socket.h"
#include "envoy/stats/scope.h"
#include "envoy/stream_info/filter_state.h"
#include "source/common/router/string_accessor_impl.h"

using Istio::Common::WorkloadMetadataObject;
using Istio::Common::WorkloadType;

namespace Envoy {
namespace WorkloadMetadata {

Config::Config(Stats::Scope& scope, const std::string& cluster_name,
               const v1::WorkloadMetadataResources& proto_config)
    : stats_{ALL_WORKLOAD_METADATA_STATS(
          POOL_COUNTER_PREFIX(scope, "workload_metadata."))},
      cluster_name_(cluster_name) {
  // TODO: update config counter
  for (const auto& resource : proto_config.workload_metadata_resources()) {
    WorkloadType workload_type = WorkloadType::Pod;
    switch (resource.workload_type()) {
      case v1::WorkloadMetadataResource_WorkloadType_KUBERNETES_DEPLOYMENT:
        workload_type = WorkloadType::Deployment;
        break;
      case v1::WorkloadMetadataResource_WorkloadType_KUBERNETES_CRONJOB:
        workload_type = WorkloadType::CronJob;
        break;
      case v1::WorkloadMetadataResource_WorkloadType_KUBERNETES_POD:
        workload_type = WorkloadType::Pod;
        break;
      case v1::WorkloadMetadataResource_WorkloadType_KUBERNETES_JOB:
        workload_type = WorkloadType::Job;
        break;
      default:
        break;
    }
    WorkloadMetadataObject workload(
        resource.instance_name(), cluster_name_, resource.namespace_name(),
        resource.workload_name(), resource.canonical_name(),
        resource.canonical_revision(), "", "", workload_type);

    for (const auto& ip_addr : resource.ip_addresses()) {
      workloads_by_ips_[ip_addr] =
          std::make_shared<WorkloadMetadataObject>(workload);
    }
  }
}

std::shared_ptr<WorkloadMetadataObject> Config::metadata(
    const std::string& ip_addr) {
  auto workload_meta = workloads_by_ips_.find(ip_addr);
  if (workload_meta != workloads_by_ips_.end()) {
    return workload_meta->second;
  }

  return nullptr;
}

Network::FilterStatus Filter::onAccept(Network::ListenerFilterCallbacks& cb) {
  ENVOY_LOG(debug, "workload metadata: new connection accepted");

  const Network::ConnectionSocket& socket = cb.socket();
  auto remote_ip =
      socket.connectionInfoProvider().remoteAddress()->ip()->addressAsString();

  ENVOY_LOG(trace, "workload metadata: looking up metadata for ip {}",
            remote_ip);

  // TODO: handle not found differently???
  auto metadata = config_->metadata(remote_ip);
  if (!metadata) {
    ENVOY_LOG(trace, "workload metadata: no metadata found for {}", remote_ip);
    return Network::FilterStatus::Continue;
  }

  ENVOY_LOG(trace, "workload metadata: found metadata for {}",
            metadata->workload_name_);

  // Setting a StringAccessor filter state with the baggage string which can be
  // assigned to custom header with PER_REQUEST_STATE.
  // We set this filter state in addition to the dynamic metadata to cover
  // cases where the dynamic metadata can not be passed through (e.g. when
  // traffic goes through an internal lisener).
  auto accessor =
      std::make_shared<Envoy::Router::StringAccessorImpl>(metadata->baggage());
  StreamInfo::FilterState& filter_state = cb.filterState();
  filter_state.setData(
      Istio::Common::kSourceMetadataBaggageKey, accessor,
      StreamInfo::FilterState::StateType::ReadOnly,
      StreamInfo::FilterState::LifeSpan::Request,
      StreamInfo::FilterState::StreamSharing::SharedWithUpstreamConnection);

  ProtobufWkt::Struct dynamic_meta;
  auto& mutable_fields = *dynamic_meta.mutable_fields();
  mutable_fields[DynamicMetadataKeysSingleton::get().Baggage].set_string_value(
      std::string(metadata->baggage()));
  cb.setDynamicMetadata(DynamicMetadataKeysSingleton::get().FilterNamespace,
                        dynamic_meta);
  return Network::FilterStatus::Continue;
}

Network::FilterStatus Filter::onData(Network::ListenerFilterBuffer&) {
  return Network::FilterStatus::Continue;
}

size_t Filter::maxReadBytes() const { return 0; }

}  // namespace WorkloadMetadata
}  // namespace Envoy
