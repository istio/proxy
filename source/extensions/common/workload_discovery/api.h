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

#include "envoy/network/address.h"
#include "envoy/stats/stats_macros.h"
#include "envoy/server/factory_context.h"
#include "extensions/common/metadata_object.h"

namespace Envoy::Extensions::Common::WorkloadDiscovery {

#define WORKLOAD_DISCOVERY_STATS(GAUGE) GAUGE(total, NeverImport)

struct WorkloadDiscoveryStats {
  WORKLOAD_DISCOVERY_STATS(GENERATE_GAUGE_STRUCT)
};

class WorkloadMetadataProvider {
public:
  virtual ~WorkloadMetadataProvider() = default;
  virtual std::optional<Istio::Common::WorkloadMetadataObject>
  GetMetadata(const Network::Address::InstanceConstSharedPtr& address) PURE;
};

using WorkloadMetadataProviderSharedPtr = std::shared_ptr<WorkloadMetadataProvider>;

WorkloadMetadataProviderSharedPtr GetProvider(Server::Configuration::ServerFactoryContext& context);

} // namespace Envoy::Extensions::Common::WorkloadDiscovery
