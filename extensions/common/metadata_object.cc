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
#include "flatbuffers/flatbuffers.h"
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
                                canonical_revision, app_name, app_version, workload_type);
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

namespace {
// Returns a string view stored in a flatbuffers string.
absl::string_view toAbslStringView(const flatbuffers::String* str) {
  return str ? absl::string_view(str->c_str(), str->size()) : absl::string_view();
}

std::string_view toStdStringView(absl::string_view view) {
  return std::string_view(view.data(), view.size());
}
} // namespace

std::string convertWorkloadMetadataToFlatNode(const WorkloadMetadataObject& obj) {
  flatbuffers::FlatBufferBuilder fbb;

  flatbuffers::Offset<flatbuffers::String> name, cluster, namespace_, workload_name, owner;
  std::vector<flatbuffers::Offset<Wasm::Common::KeyVal>> labels;

  name = fbb.CreateString(toStdStringView(obj.instance_name_));
  namespace_ = fbb.CreateString(toStdStringView(obj.namespace_name_));
  cluster = fbb.CreateString(toStdStringView(obj.cluster_name_));
  workload_name = fbb.CreateString(toStdStringView(obj.workload_name_));

  switch (obj.workload_type_) {
  case WorkloadType::Deployment:
    owner = fbb.CreateString(absl::StrCat(OwnerPrefix, obj.namespace_name_, "/", DeploymentSuffix,
                                          "s/", obj.workload_name_));
    break;
  case WorkloadType::Job:
    owner = fbb.CreateString(
        absl::StrCat(OwnerPrefix, obj.namespace_name_, "/", JobSuffix, "s/", obj.workload_name_));
    break;
  case WorkloadType::CronJob:
    owner = fbb.CreateString(absl::StrCat(OwnerPrefix, obj.namespace_name_, "/", CronJobSuffix,
                                          "s/", obj.workload_name_));
    break;
  case WorkloadType::Pod:
    owner = fbb.CreateString(
        absl::StrCat(OwnerPrefix, obj.namespace_name_, "/", PodSuffix, "s/", obj.workload_name_));
    break;
  }

  labels.push_back(
      Wasm::Common::CreateKeyVal(fbb, fbb.CreateString("service.istio.io/canonical-name"),
                                 fbb.CreateString(toStdStringView(obj.canonical_name_))));
  labels.push_back(
      Wasm::Common::CreateKeyVal(fbb, fbb.CreateString("service.istio.io/canonical-revision"),
                                 fbb.CreateString(toStdStringView(obj.canonical_revision_))));
  labels.push_back(Wasm::Common::CreateKeyVal(fbb, fbb.CreateString("app"),
                                              fbb.CreateString(toStdStringView(obj.app_name_))));
  labels.push_back(Wasm::Common::CreateKeyVal(fbb, fbb.CreateString("version"),
                                              fbb.CreateString(toStdStringView(obj.app_version_))));

  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  Wasm::Common::FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_cluster_id(cluster);
  node.add_namespace_(namespace_);
  node.add_workload_name(workload_name);
  node.add_owner(owner);
  node.add_labels(labels_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  auto fb = fbb.Release();
  return std::string(reinterpret_cast<const char*>(fb.data()), fb.size());
}

WorkloadMetadataObject convertFlatNodeToWorkloadMetadata(const Wasm::Common::FlatNode& node) {
  const absl::string_view instance = toAbslStringView(node.name());
  const absl::string_view cluster = toAbslStringView(node.cluster_id());
  const absl::string_view workload = toAbslStringView(node.workload_name());
  const absl::string_view namespace_name = toAbslStringView(node.namespace_());
  const auto* labels = node.labels();

  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  absl::string_view app_name;
  absl::string_view app_version;
  if (labels) {
    const auto* name_iter = labels->LookupByKey("service.istio.io/canonical-name");
    const auto* name = name_iter ? name_iter->value() : nullptr;
    canonical_name = toAbslStringView(name);

    const auto* revision_iter = labels->LookupByKey("service.istio.io/canonical-revision");
    const auto* revision = revision_iter ? revision_iter->value() : nullptr;
    canonical_revision = toAbslStringView(revision);

    const auto* app_iter = labels->LookupByKey("app");
    const auto* app = app_iter ? app_iter->value() : nullptr;
    app_name = toAbslStringView(app);

    const auto* version_iter = labels->LookupByKey("version");
    const auto* version = version_iter ? version_iter->value() : nullptr;
    app_version = toAbslStringView(version);
  }

  WorkloadType workload_type = WorkloadType::Pod;
  // Strip "s/workload_name" and check for workload type.
  absl::string_view owner = toAbslStringView(node.owner());
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

  return WorkloadMetadataObject(instance, cluster, namespace_name, workload, canonical_name,
                                canonical_revision, app_name, app_version, workload_type);
}

absl::optional<WorkloadMetadataObject>
convertEndpointMetadata(const std::string& endpoint_encoding) {
  std::vector<absl::string_view> parts = absl::StrSplit(endpoint_encoding, ';');
  if (parts.size() < 5) {
    return {};
  }
  // TODO: we cannot determine workload type from the encoding.
  return absl::make_optional<WorkloadMetadataObject>("", parts[4], parts[1], parts[0], parts[2],
                                                     parts[3], "", "", WorkloadType::Pod);
}

} // namespace Common
} // namespace Istio
