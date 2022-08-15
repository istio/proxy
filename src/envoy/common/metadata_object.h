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
  static constexpr absl::string_view kSourceMetadataBaggageKey =
      "ambient.source.workloadMetadataBaggage";
  static constexpr absl::string_view kDestinationMetadataObjectKey =
      "ambient.destination.workloadMetadata";

  WorkloadMetadataObject() : workload_type_(WorkloadType::KUBERNETES_POD) {}

  WorkloadMetadataObject(
      const std::string& instance_name, const std::string& cluster_name,
      const std::string& namespace_name, const std::string& workload_name,
      const std::string& canonical_name, const std::string& canonical_revision,
      const WorkloadType workload_type,
      const std::vector<std::string>& ip_addresses,
      const std::vector<std::string>& containers,
      const Ssl::ConnectionInfoConstSharedPtr& ssl_conn_info = nullptr)
      : instance_name_(instance_name),
        cluster_(cluster_name),
        namespace_(namespace_name),
        workload_name_(workload_name),
        canonical_name_(canonical_name),
        canonical_revision_(canonical_revision),
        workload_type_(workload_type),
        ip_addresses_(ip_addresses),
        containers_(containers),
        baggage_(getBaggage()),
        ssl_conn_info_(ssl_conn_info) {}

  static std::shared_ptr<WorkloadMetadataObject> fromBaggage(
      const absl::string_view baggage_header_value,
      const Ssl::ConnectionInfoConstSharedPtr& ssl_conn_info = nullptr) {
    // TODO: check for well-formed-ness of the baggage string

    std::string instance;
    std::string cluster;
    std::string workload;
    std::string namespace_name;
    std::string canonical_name;
    std::string canonical_revision;
    WorkloadType workload_type;

    std::vector<absl::string_view> properties =
        absl::StrSplit(baggage_header_value, ',');
    for (absl::string_view property : properties) {
      std::pair<absl::string_view, const std::string> parts =
          absl::StrSplit(property, "=");
      if (parts.first == "k8s.namespace.name") {
        namespace_name = parts.second;
      } else if (parts.first == "k8s.cluster.name") {
        cluster = parts.second;
      } else if (parts.first == "service.name") {
        canonical_name = parts.second;
      } else if (parts.first == "service.version") {
        canonical_revision = parts.second;
      } else if (parts.first == "k8s.pod.name") {
        workload_type = WorkloadType::KUBERNETES_POD;
        instance = parts.second;
        workload = parts.second;
      } else if (parts.first == "k8s.deployment.name") {
        workload_type = WorkloadType::KUBERNETES_DEPLOYMENT;
        workload = parts.second;
      } else if (parts.first == "k8s.job.name") {
        workload_type = WorkloadType::KUBERNETES_JOB;
        instance = parts.second;
        workload = parts.second;
      } else if (parts.first == "k8s.cronjob.name") {
        workload_type = WorkloadType::KUBERNETES_CRONJOB;
        workload = parts.second;
      }
    }
    return std::make_shared<WorkloadMetadataObject>(WorkloadMetadataObject(
        instance, cluster, namespace_name, workload, canonical_name,
        canonical_revision, workload_type, {}, {}, ssl_conn_info));
  }

  absl::string_view instanceName() const { return instance_name_; }
  absl::string_view clusterName() const { return cluster_; }
  absl::string_view namespaceName() const { return namespace_; }
  absl::string_view workloadName() const { return workload_name_; }
  absl::string_view canonicalName() const { return canonical_name_; }
  absl::string_view canonicalRevision() const { return canonical_revision_; }
  WorkloadType workloadType() const { return workload_type_; }
  const std::vector<std::string>& ipAddresses() const { return ip_addresses_; }
  const std::vector<std::string>& containers() const { return containers_; }
  absl::string_view baggage() const { return baggage_; }
  Ssl::ConnectionInfoConstSharedPtr ssl() const { return ssl_conn_info_; }

  absl::optional<uint64_t> hash() const override;

  absl::optional<std::string> serializeAsString() const override {
    return std::string{baggage_};
  }

 private:
  // TODO: cloud.account.id and k8s.cluster.name
  static constexpr absl::string_view kBaggageFormat =
      "k8s.cluster.name=%s,k8s.namespace.name=%s,k8s.%s.name=%s,service.name=%"
      "s,service.version=%s";

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
    return absl::StrFormat(kBaggageFormat, cluster_, namespace_, wlType,
                           workload_name_, canonical_name_,
                           canonical_revision_);
  }

  const std::string instance_name_;
  const std::string cluster_;
  const std::string namespace_;
  const std::string workload_name_;
  const std::string canonical_name_;
  const std::string canonical_revision_;
  const WorkloadType workload_type_;
  const std::vector<std::string> ip_addresses_;
  const std::vector<std::string> containers_;
  const std::string baggage_;

  const Ssl::ConnectionInfoConstSharedPtr ssl_conn_info_;
};

}  // namespace Common
}  // namespace Envoy
