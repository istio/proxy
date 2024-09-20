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

#include "envoy/common/hashable.h"
#include "envoy/stream_info/filter_state.h"

#include "absl/strings/str_split.h"
#include "absl/types/optional.h"

#include "google/protobuf/struct.pb.h"

namespace Istio {
namespace Common {

// Filter state key to store the peer metadata under.
constexpr absl::string_view DownstreamPeer = "downstream_peer";
constexpr absl::string_view UpstreamPeer = "upstream_peer";

// Special filter state key to indicate the filter is done looking for peer metadata.
// This is used by network metadata exchange on failure.
constexpr absl::string_view NoPeer = "peer_not_found";

// Special labels used in the peer metadata.
constexpr absl::string_view CanonicalNameLabel = "service.istio.io/canonical-name";
constexpr absl::string_view CanonicalRevisionLabel = "service.istio.io/canonical-revision";

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
  WorkloadName,
  WorkloadType,
  InstanceName,
};

constexpr absl::string_view NamespaceNameToken = "namespace";
constexpr absl::string_view ClusterNameToken = "cluster";
constexpr absl::string_view ServiceNameToken = "service";
constexpr absl::string_view ServiceVersionToken = "version";
constexpr absl::string_view WorkloadNameToken = "workload";
constexpr absl::string_view WorkloadTypeToken = "type";
constexpr absl::string_view InstanceNameToken = "name";

struct WorkloadMetadataObject : public Envoy::StreamInfo::FilterState::Object,
                                public Envoy::Hashable {
  explicit WorkloadMetadataObject(absl::string_view instance_name, absl::string_view cluster_name,
                                  absl::string_view namespace_name, absl::string_view workload_name,
                                  absl::string_view canonical_name,
                                  absl::string_view canonical_revision,
                                  const WorkloadType workload_type, absl::string_view identity)
      : instance_name_(instance_name), cluster_name_(cluster_name), namespace_name_(namespace_name),
        workload_name_(workload_name), canonical_name_(canonical_name),
        canonical_revision_(canonical_revision), workload_type_(workload_type),
        identity_(identity) {}

  absl::optional<uint64_t> hash() const override;
  absl::optional<std::string> serializeAsString() const override;

  const std::string instance_name_;
  const std::string cluster_name_;
  const std::string namespace_name_;
  const std::string workload_name_;
  const std::string canonical_name_;
  const std::string canonical_revision_;
  const WorkloadType workload_type_;
  const std::string identity_;
};

// Convert a metadata object to a struct.
google::protobuf::Struct convertWorkloadMetadataToStruct(const WorkloadMetadataObject& obj);

// Convert struct to a metadata object.
std::unique_ptr<WorkloadMetadataObject>
convertStructToWorkloadMetadata(const google::protobuf::Struct& metadata);

// Convert endpoint metadata string to a metadata object.
// Telemetry metadata is compressed into a semicolon separated string:
// workload-name;namespace;canonical-service-name;canonical-service-revision;cluster-id.
// Telemetry metadata is stored as a string under "istio", "workload" field
// path.
absl::optional<WorkloadMetadataObject>
convertEndpointMetadata(const std::string& endpoint_encoding);

std::string serializeToStringDeterministic(const google::protobuf::Struct& metadata);

class WorkloadMetadataObjectFactory : public Envoy::StreamInfo::FilterState::ObjectFactory {
public:
  std::unique_ptr<Envoy::StreamInfo::FilterState::Object>
  createFromBytes(absl::string_view data) const override;
  std::unique_ptr<Envoy::StreamInfo::FilterState::ObjectReflection>
  reflect(const Envoy::StreamInfo::FilterState::Object* data) const override;
};

} // namespace Common
} // namespace Istio
