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

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/types/optional.h"
#include "envoy/common/hashable.h"
#include "envoy/ssl/connection.h"
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Common {

enum class WorkloadType {
  UNKNOWN,
  KUBERNETES_DEPLOYMENT,
  KUBERNETES_CRONJOB,
  KUBERNETES_POD,
  KUBERNETES_JOB,
};

class WorkloadMetadataObject : public StreamInfo::FilterState::Object,
                               public Hashable {
 public:
  static constexpr absl::string_view kSourceMetadataObjectKey =
      "ambient.source.workloadMetadata";
  static constexpr absl::string_view kSourceMetadataBaggageKey =
      "ambient.source.workloadMetadataBaggage";
  static constexpr absl::string_view kDestinationMetadataObjectKey =
      "ambient.destination.workloadMetadata";

  explicit WorkloadMetadataObject(absl::string_view instance_name,
                                  absl::string_view cluster_name,
                                  absl::string_view namespace_name,
                                  absl::string_view workload_name,
                                  absl::string_view canonical_name,
                                  absl::string_view canonical_revision,
                                  const WorkloadType workload_type,
                                  const std::vector<std::string>& ip_addresses,
                                  const std::vector<std::string>& containers)
      : instance_name_(instance_name),
        cluster_(cluster_name),
        namespace_(namespace_name),
        workload_name_(workload_name),
        canonical_name_(canonical_name),
        canonical_revision_(canonical_revision),
        workload_type_(workload_type),
        ip_addresses_(ip_addresses),
        containers_(containers) {}

  static std::shared_ptr<WorkloadMetadataObject> fromBaggage(
      absl::string_view baggage_header_value);

  absl::string_view instanceName() const { return instance_name_; }
  absl::string_view clusterName() const { return cluster_; }
  absl::string_view namespaceName() const { return namespace_; }
  absl::string_view workloadName() const { return workload_name_; }
  absl::string_view canonicalName() const { return canonical_name_; }
  absl::string_view canonicalRevision() const { return canonical_revision_; }
  WorkloadType workloadType() const { return workload_type_; }
  const std::vector<std::string>& ipAddresses() const { return ip_addresses_; }
  const std::vector<std::string>& containers() const { return containers_; }

  std::string baggage() const;

  absl::optional<uint64_t> hash() const override;

  absl::optional<std::string> serializeAsString() const override {
    return baggage();
  }

 private:
  const std::string instance_name_;
  const std::string cluster_;
  const std::string namespace_;
  const std::string workload_name_;
  const std::string canonical_name_;
  const std::string canonical_revision_;
  const WorkloadType workload_type_;
  const std::vector<std::string> ip_addresses_;
  const std::vector<std::string> containers_;
};

}  // namespace Common
}  // namespace Envoy
