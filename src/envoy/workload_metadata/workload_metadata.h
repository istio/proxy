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

#include "envoy/common/optref.h"
#include "envoy/network/filter.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"
#include "extensions/common/metadata_object.h"
#include "source/common/common/logger.h"
#include "src/envoy/workload_metadata/config/workload_metadata.pb.h"

using namespace istio::telemetry::workloadmetadata;
using Istio::Common::WorkloadMetadataObject;

namespace Envoy {
namespace WorkloadMetadata {

/**
 * All stats for the workload metadata filter. @see stats_macros.h
 */
#define ALL_WORKLOAD_METADATA_STATS(COUNTER) \
  COUNTER(config_error)                      \
  COUNTER(config_updates)

/**
 * Definition of all stats for the Workload Metadata filter. @see stats_macros.h
 */
struct WorkloadMetadataStats {
  ALL_WORKLOAD_METADATA_STATS(GENERATE_COUNTER_STRUCT)
};

/**
 * Definition of keys in the dynamic metadata to store baggage in
 */
class DynamicMetadataKeys {
 public:
  const std::string FilterNamespace{"envoy.filters.listener.workload_metadata"};
  const std::string Baggage{"baggage"};
};

using DynamicMetadataKeysSingleton = ConstSingleton<DynamicMetadataKeys>;

/**
 * Global configuration for Workload Metadata listener filter.
 */
class Config : public Logger::Loggable<Logger::Id::filter> {
 public:
  Config(Stats::Scope& scope, const std::string& cluster_name,
         const v1::WorkloadMetadataResources& proto_config);

  const WorkloadMetadataStats& stats() const { return stats_; }

  std::shared_ptr<WorkloadMetadataObject> metadata(const std::string& ip_addr);

 private:
  WorkloadMetadataStats stats_;
  const std::string cluster_name_;
  absl::flat_hash_map<std::string, std::shared_ptr<WorkloadMetadataObject>>
      workloads_by_ips_;
};

using ConfigSharedPtr = std::shared_ptr<Config>;

/**
 * Workload Metadata listener filter.
 */
class Filter : public Network::ListenerFilter,
               Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(const ConfigSharedPtr& config) : config_(config) {}

  // Network::ListenerFilter
  Network::FilterStatus onAccept(Network::ListenerFilterCallbacks& cb) override;

  Network::FilterStatus onData(Network::ListenerFilterBuffer&) override;

  size_t maxReadBytes() const override;

 private:
  ConfigSharedPtr config_;
};

}  // namespace WorkloadMetadata
}  // namespace Envoy
