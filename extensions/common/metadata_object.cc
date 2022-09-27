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

#include "extensions/common/metadata_object.h"

#include "source/common/common/hash.h"

namespace Istio {
namespace Common {

WorkloadMetadataObject WorkloadMetadataObject::fromBaggage(
    absl::string_view baggage_header_value) {
  // TODO: check for well-formed-ness of the baggage string: duplication,
  // inconsistency
  absl::string_view instance;
  absl::string_view cluster;
  absl::string_view workload;
  absl::string_view namespace_name;
  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  absl::string_view app_name;
  absl::string_view app_version;
  WorkloadType workload_type = WorkloadType::KUBERNETES_POD;
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
    } else if (parts.first == "app.name") {
      app_name = parts.second;
    } else if (parts.first == "app.version") {
      app_version = parts.second;
    }
  }
  return WorkloadMetadataObject(instance, cluster, namespace_name, workload,
                                canonical_name, canonical_revision, app_name,
                                app_version, workload_type, empty, empty);
}

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
  return absl::StrCat("k8s.cluster.name=", cluster_name_,
                      ",k8s.namespace.name=", namespace_name_, ",k8s.", wlType,
                      ".name=", workload_name_,
                      ",service.name=", canonical_name_,
                      ",service.version=", canonical_revision_,
                      ",app.name=", app_name_, ",app.version=", app_version_);
}

absl::optional<uint64_t> WorkloadMetadataObject::hash() const {
  return Envoy::HashUtil::xxHash64(
      absl::StrCat(instance_name_, "/", namespace_name_));
}

}  // namespace Common
}  // namespace Istio
