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
#include "absl/types/optional.h"
#include "envoy/common/hashable.h"
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Common {

enum class WorkloadType {
  KUBERNETES_DEPLOYMENT,
  KUBERNETES_CRONJOB,
  KUBERNETES_POD,
  KUBERNETES_JOB,
};

class WorkloadMetadataObject : public Envoy::StreamInfo::FilterState::Object,
                               public Envoy::Hashable {
 public:
  static constexpr absl::string_view kSourceMetadataObjectKey =
      "ambient.source.workloadMetadata";
  static constexpr absl::string_view kDestinationMetadataObjectKey =
      "ambient.destination.workloadMetadata";

  WorkloadMetadataObject() : workload_type_(WorkloadType::KUBERNETES_POD) {}

  WorkloadMetadataObject(const std::string& instance_name,
                         const std::string& namespace_name,
                         const std::string& workload_name,
                         const std::string& canonical_name,
                         const std::string& canonical_revision,
                         const WorkloadType workload_type,
                         const std::vector<std::string>& ip_addresses,
                         const std::vector<std::string>& containers)
      : instance_name_(instance_name),
        namespace_(namespace_name),
        workload_name_(workload_name),
        canonical_name_(canonical_name),
        canonical_revision_(canonical_revision),
        workload_type_(workload_type),
        ip_addresses_(ip_addresses),
        containers_(containers),
        baggage_(getBaggage()) {}

  absl::string_view instanceName() const { return instance_name_; }
  absl::string_view namespaceName() const { return namespace_; }
  absl::string_view workloadName() const { return workload_name_; }
  absl::string_view canonicalName() const { return canonical_name_; }
  absl::string_view canonicalRevision() const { return canonical_revision_; }
  WorkloadType workloadType() const { return workload_type_; }
  const std::vector<std::string>& ipAddresses() const { return ip_addresses_; }
  const std::vector<std::string>& containers() const { return containers_; }
  absl::string_view baggage() const { return baggage_; }

  absl::optional<uint64_t> hash() const override;

 private:
  // TODO: cloud.account.id and k8s.cluster.name
  static constexpr absl::string_view kBaggageFormat =
      "k8s.namespace.name=%s,k8s.%s.name=%s,service.name=%s,service.version=%s";

  const std::string getBaggage() {
    absl::string_view wlType = "pod";
    switch (workload_type_) {
      case WorkloadType::KUBERNETES_DEPLOYMENT:
        wlType = "deployment";
        break;
      case WorkloadType::KUBERNETES_CRONJOB:
        wlType = "cronjob";
        break;
      case WorkloadType::KUBERNETES_JOB:
        wlType = "job";
        break;
      case WorkloadType::KUBERNETES_POD:
        wlType = "pod";
        break;
      default:
        wlType = "pod";
    }
    return absl::StrFormat(kBaggageFormat, namespace_, wlType, workload_name_,
                           canonical_name_, canonical_revision_);
  }

  const std::string instance_name_;
  const std::string namespace_;
  const std::string workload_name_;
  const std::string canonical_name_;
  const std::string canonical_revision_;
  const WorkloadType workload_type_;
  const std::vector<std::string> ip_addresses_;
  const std::vector<std::string> containers_;
  const std::string baggage_;
};

}  // namespace Common
}  // namespace Envoy
