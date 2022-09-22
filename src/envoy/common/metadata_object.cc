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

#include "src/envoy/common/metadata_object.h"

#include "absl/strings/str_join.h"
#include "source/common/common/hash.h"

namespace Envoy {
namespace Common {

std::shared_ptr<WorkloadMetadataObject> WorkloadMetadataObject::fromBaggage(
    absl::string_view baggage_header_value) {
  // TODO: check for well-formed-ness of the baggage string

  absl::string_view instance;
  absl::string_view cluster;
  absl::string_view workload;
  absl::string_view namespace_name;
  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  WorkloadType workload_type = WorkloadType::UNKNOWN;
  const std::vector<std::string> empty;

  std::vector<absl::string_view> properties =
      absl::StrSplit(baggage_header_value, ',');
  for (absl::string_view property : properties) {
    std::pair<absl::string_view, absl::string_view> parts =
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
  return std::make_shared<WorkloadMetadataObject>(
      instance, cluster, namespace_name, workload, canonical_name,
      canonical_revision, workload_type, empty, empty);
}

// TODO: cloud.account.id and k8s.cluster.name
static constexpr absl::string_view kBaggageFormat =
    "k8s.cluster.name=%s,k8s.namespace.name=%s,k8s.%s.name=%s,service.name=%"
    "s,service.version=%s";

std::string WorkloadMetadataObject::baggage() const {
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
                         workload_name_, canonical_name_, canonical_revision_);
}

absl::optional<uint64_t> WorkloadMetadataObject::hash() const {
  return Envoy::HashUtil::xxHash64(
      absl::StrCat(instance_name_, "/", namespace_));
}

}  // namespace Common
}  // namespace Envoy
