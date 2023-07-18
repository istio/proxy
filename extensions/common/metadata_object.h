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

#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "envoy/common/hashable.h"
#include "envoy/ssl/connection.h"
#include "envoy/stream_info/filter_state.h"
#include "extensions/common/node_info_generated.h"

namespace Istio {
namespace Common {

enum class WorkloadType {
  Pod,
  Deployment,
  Job,
  CronJob,
};

constexpr absl::string_view OwnerPrefix = "kubernetes://apis/apps/v1/namespaces/";
constexpr absl::string_view PodSuffix = "pod";
constexpr absl::string_view DeploymentSuffix = "deployment";
constexpr absl::string_view JobSuffix = "job";
constexpr absl::string_view CronJobSuffix = "cronjob";

enum class BaggageToken {
  NamespaceName,
  ClusterName,
  ServiceName,
  ServiceVersion,
  PodName,
  DeploymentName,
  JobName,
  CronJobName,
  AppName,
  AppVersion,
};

constexpr absl::string_view NamespaceNameToken = "k8s.namespace.name";
constexpr absl::string_view ClusterNameToken = "k8s.cluster.name";
constexpr absl::string_view ServiceNameToken = "service.name";
constexpr absl::string_view ServiceVersionToken = "service.version";
constexpr absl::string_view PodNameToken = "k8s.pod.name";
constexpr absl::string_view DeploymentNameToken = "k8s.deployment.name";
constexpr absl::string_view JobNameToken = "k8s.job.name";
constexpr absl::string_view CronJobNameToken = "k8s.cronjob.name";
constexpr absl::string_view AppNameToken = "app.name";
constexpr absl::string_view AppVersionToken = "app.version";

constexpr absl::string_view kSourceMetadataObjectKey = "ambient.source.workloadMetadata";
constexpr absl::string_view kSourceMetadataBaggageKey = "ambient.source.workloadMetadataBaggage";
constexpr absl::string_view kDestinationMetadataObjectKey = "ambient.destination.workloadMetadata";

struct WorkloadMetadataObject : public Envoy::StreamInfo::FilterState::Object,
                                public Envoy::Hashable {
  explicit WorkloadMetadataObject(absl::string_view instance_name, absl::string_view cluster_name,
                                  absl::string_view namespace_name, absl::string_view workload_name,
                                  absl::string_view canonical_name,
                                  absl::string_view canonical_revision, absl::string_view app_name,
                                  absl::string_view app_version, const WorkloadType workload_type)
      : instance_name_(instance_name), cluster_name_(cluster_name), namespace_name_(namespace_name),
        workload_name_(workload_name), canonical_name_(canonical_name),
        canonical_revision_(canonical_revision), app_name_(app_name), app_version_(app_version),
        workload_type_(workload_type) {}

  static WorkloadMetadataObject fromBaggage(absl::string_view baggage_header_value);

  std::string baggage() const;

  absl::optional<uint64_t> hash() const override;

  absl::optional<std::string> serializeAsString() const override { return baggage(); }

  const std::string instance_name_;
  const std::string cluster_name_;
  const std::string namespace_name_;
  const std::string workload_name_;
  const std::string canonical_name_;
  const std::string canonical_revision_;
  const std::string app_name_;
  const std::string app_version_;
  const WorkloadType workload_type_;
};

// Convert metadata object to flatbuffer.
std::string convertWorkloadMetadataToFlatNode(const WorkloadMetadataObject& obj);

// Convert flatbuffer to metadata object.
WorkloadMetadataObject convertFlatNodeToWorkloadMetadata(const Wasm::Common::FlatNode& node);

// Convert endpoint metadata string to a metadata object.
// Telemetry metadata is compressed into a semicolon separated string:
// workload-name;namespace;canonical-service-name;canonical-service-revision;cluster-id.
// Telemetry metadata is stored as a string under "istio", "workload" field
// path.
absl::optional<WorkloadMetadataObject>
convertEndpointMetadata(const std::string& endpoint_encoding);

} // namespace Common
} // namespace Istio
