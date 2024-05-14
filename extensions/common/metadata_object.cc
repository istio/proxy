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

#include "absl/strings/str_join.h"
#include "source/common/common/hash.h"

namespace Istio {
namespace Common {

static absl::flat_hash_map<absl::string_view, BaggageToken> ALL_BAGGAGE_TOKENS = {
    {NamespaceNameToken, BaggageToken::NamespaceName},
    {ClusterNameToken, BaggageToken::ClusterName},
    {ServiceNameToken, BaggageToken::ServiceName},
    {ServiceVersionToken, BaggageToken::ServiceVersion},
    {PodNameToken, BaggageToken::PodName},
    {DeploymentNameToken, BaggageToken::DeploymentName},
    {JobNameToken, BaggageToken::JobName},
    {CronJobNameToken, BaggageToken::CronJobName},
    {AppNameToken, BaggageToken::AppName},
    {AppVersionToken, BaggageToken::AppVersion},
};

static absl::flat_hash_map<absl::string_view, WorkloadType> ALL_WORKLOAD_TOKENS = {
    {PodSuffix, WorkloadType::Pod},
    {DeploymentSuffix, WorkloadType::Deployment},
    {JobSuffix, WorkloadType::Job},
    {CronJobSuffix, WorkloadType::CronJob},
};

WorkloadMetadataObject WorkloadMetadataObject::fromBaggage(absl::string_view baggage_header_value) {
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
  WorkloadType workload_type = WorkloadType::Pod;

  std::vector<absl::string_view> properties = absl::StrSplit(baggage_header_value, ',');
  for (absl::string_view property : properties) {
    std::pair<absl::string_view, absl::string_view> parts = absl::StrSplit(property, "=");
    const auto it = ALL_BAGGAGE_TOKENS.find(parts.first);
    if (it != ALL_BAGGAGE_TOKENS.end()) {
      switch (it->second) {
      case BaggageToken::NamespaceName:
        namespace_name = parts.second;
        break;
      case BaggageToken::ClusterName:
        cluster = parts.second;
        break;
      case BaggageToken::ServiceName:
        canonical_name = parts.second;
        break;
      case BaggageToken::ServiceVersion:
        canonical_revision = parts.second;
        break;
      case BaggageToken::PodName:
        workload_type = WorkloadType::Pod;
        instance = parts.second;
        workload = parts.second;
        break;
      case BaggageToken::DeploymentName:
        workload_type = WorkloadType::Deployment;
        workload = parts.second;
        break;
      case BaggageToken::JobName:
        workload_type = WorkloadType::Job;
        instance = parts.second;
        workload = parts.second;
        break;
      case BaggageToken::CronJobName:
        workload_type = WorkloadType::CronJob;
        workload = parts.second;
        break;
      case BaggageToken::AppName:
        app_name = parts.second;
        break;
      case BaggageToken::AppVersion:
        app_version = parts.second;
        break;
      }
    }
  }
  return WorkloadMetadataObject(instance, cluster, namespace_name, workload, canonical_name,
                                canonical_revision, app_name, app_version, workload_type, "");
}

std::string WorkloadMetadataObject::baggage() const {
  absl::string_view workload_type = PodSuffix;
  switch (workload_type_) {
  case WorkloadType::Deployment:
    workload_type = DeploymentSuffix;
    break;
  case WorkloadType::CronJob:
    workload_type = CronJobSuffix;
    break;
  case WorkloadType::Job:
    workload_type = JobSuffix;
    break;
  case WorkloadType::Pod:
    workload_type = PodSuffix;
    break;
  default:
    break;
  }
  std::vector<absl::string_view> parts;
  parts.push_back("k8s.");
  parts.push_back(workload_type);
  parts.push_back(".name=");
  parts.push_back(workload_name_);
  if (!cluster_name_.empty()) {
    parts.push_back(",");
    parts.push_back(ClusterNameToken);
    parts.push_back("=");
    parts.push_back(cluster_name_);
  }
  if (!namespace_name_.empty()) {
    parts.push_back(",");
    parts.push_back(NamespaceNameToken);
    parts.push_back("=");
    parts.push_back(namespace_name_);
  }
  if (!canonical_name_.empty()) {
    parts.push_back(",");
    parts.push_back(ServiceNameToken);
    parts.push_back("=");
    parts.push_back(canonical_name_);
  }
  if (!canonical_revision_.empty()) {
    parts.push_back(",");
    parts.push_back(ServiceVersionToken);
    parts.push_back("=");
    parts.push_back(canonical_revision_);
  }
  if (!app_name_.empty()) {
    parts.push_back(",");
    parts.push_back(AppNameToken);
    parts.push_back("=");
    parts.push_back(app_name_);
  }
  if (!app_version_.empty()) {
    parts.push_back(",");
    parts.push_back(AppVersionToken);
    parts.push_back("=");
    parts.push_back(app_version_);
  }
  return absl::StrJoin(parts, "");
}

absl::optional<uint64_t> WorkloadMetadataObject::hash() const {
  return Envoy::HashUtil::xxHash64(absl::StrCat(instance_name_, "/", namespace_name_));
}

std::string WorkloadMetadataObject::owner() const {
  switch (workload_type_) {
  case WorkloadType::Deployment:
    return absl::StrCat(OwnerPrefix, namespace_name_, "/", DeploymentSuffix, "s/", workload_name_);
  case WorkloadType::Job:
    return absl::StrCat(OwnerPrefix, namespace_name_, "/", JobSuffix, "s/", workload_name_);
  case WorkloadType::CronJob:
    return absl::StrCat(OwnerPrefix, namespace_name_, "/", CronJobSuffix, "s/", workload_name_);
  case WorkloadType::Pod:
    return absl::StrCat(OwnerPrefix, namespace_name_, "/", PodSuffix, "s/", workload_name_);
  }
}

void WorkloadMetadataObject::toStruct(google::protobuf::Struct* out) const {
  if (!instance_name_.empty()) {
    (*out->mutable_fields())["NAME"].set_string_value(instance_name_);
  }
  if (!cluster_name_.empty()) {
    (*out->mutable_fields())["CLUSTER_ID"].set_string_value(cluster_name_);
  }
  if (!namespace_name_.empty()) {
    (*out->mutable_fields())["NAMESPACE"].set_string_value(namespace_name_);
  }
  if (!workload_name_.empty()) {
    (*out->mutable_fields())["WORKLOAD_NAME"].set_string_value(workload_name_);
  }
  (*out->mutable_fields())["OWNER"].set_string_value(owner());
  auto* map = (*out->mutable_fields())["LABELS"].mutable_struct_value();
  if (!canonical_name_.empty()) {
    (*map->mutable_fields())[CanonicalNameLabel].set_string_value(canonical_name_);
  }
  if (!canonical_revision_.empty()) {
    (*map->mutable_fields())[CanonicalRevisionLabel].set_string_value(canonical_revision_);
  }
  if (!app_name_.empty()) {
    (*map->mutable_fields())[AppLabel].set_string_value(app_name_);
  }
  if (!app_version_.empty()) {
    (*map->mutable_fields())[VersionLabel].set_string_value(app_version_);
  }
}

std::shared_ptr<WorkloadMetadataObject>
convertStructToWorkloadMetadata(const google::protobuf::Struct& metadata) {
  absl::string_view instance;
  absl::string_view cluster;
  absl::string_view workload;
  absl::string_view namespace_name;
  absl::string_view identity;
  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  absl::string_view app_name;
  absl::string_view app_version;
  absl::string_view owner;

  for (const auto& it : metadata.fields()) {
    if (it.first == "NAME") {
      instance = it.second.string_value();
    } else if (it.first == "NAMESPACE") {
      namespace_name = it.second.string_value();
    } else if (it.first == "OWNER") {
      owner = it.second.string_value();
    } else if (it.first == "WORKLOAD_NAME") {
      workload = it.second.string_value();
    } else if (it.first == "CLUSTER_ID") {
      cluster = it.second.string_value();
    } else if (it.first == "LABELS") {
      for (const auto& labels_it : it.second.struct_value().fields()) {
        if (labels_it.first == CanonicalNameLabel) {
          canonical_name = labels_it.second.string_value();
        } else if (labels_it.first == CanonicalRevisionLabel) {
          canonical_revision = labels_it.second.string_value();
        } else if (labels_it.first == AppLabel) {
          app_name = labels_it.second.string_value();
        } else if (labels_it.first == VersionLabel) {
          app_version = labels_it.second.string_value();
        }
      }
    }
  }

  WorkloadType workload_type = WorkloadType::Pod;
  // Strip "s/workload_name" and check for workload type.
  if (owner.size() > workload.size() + 2) {
    owner.remove_suffix(workload.size() + 2);
    size_t last = owner.rfind('/');
    if (last != absl::string_view::npos) {
      const auto it = ALL_WORKLOAD_TOKENS.find(owner.substr(last + 1));
      if (it != ALL_WORKLOAD_TOKENS.end()) {
        switch (it->second) {
        case WorkloadType::Deployment:
          workload_type = WorkloadType::Deployment;
          break;
        case WorkloadType::CronJob:
          workload_type = WorkloadType::CronJob;
          break;
        case WorkloadType::Job:
          workload_type = WorkloadType::Job;
          break;
        case WorkloadType::Pod:
          workload_type = WorkloadType::Pod;
          break;
        default:
          break;
        }
      }
    }
  }

  return std::make_shared<WorkloadMetadataObject>(instance, cluster, namespace_name, workload,
                                                  canonical_name, canonical_revision, app_name,
                                                  app_version, workload_type, identity);
}

absl::optional<WorkloadMetadataObject>
convertEndpointMetadata(const std::string& endpoint_encoding) {
  std::vector<absl::string_view> parts = absl::StrSplit(endpoint_encoding, ';');
  if (parts.size() < 5) {
    return {};
  }
  // TODO: we cannot determine workload type from the encoding.
  return absl::make_optional<WorkloadMetadataObject>("", parts[4], parts[1], parts[0], parts[2],
                                                     parts[3], "", "", WorkloadType::Pod, "");
}

} // namespace Common
} // namespace Istio
