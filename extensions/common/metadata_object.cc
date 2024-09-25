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

#include "envoy/registry/registry.h"
#include "source/common/common/hash.h"

#include "absl/strings/str_join.h"

namespace Istio {
namespace Common {

static absl::flat_hash_map<absl::string_view, BaggageToken> ALL_BAGGAGE_TOKENS = {
    {NamespaceNameToken, BaggageToken::NamespaceName},
    {ClusterNameToken, BaggageToken::ClusterName},
    {ServiceNameToken, BaggageToken::ServiceName},
    {ServiceVersionToken, BaggageToken::ServiceVersion},
    {AppNameToken, BaggageToken::AppName},
    {AppVersionToken, BaggageToken::AppVersion},
    {WorkloadNameToken, BaggageToken::WorkloadName},
    {WorkloadTypeToken, BaggageToken::WorkloadType},
    {InstanceNameToken, BaggageToken::InstanceName},
};

static absl::flat_hash_map<absl::string_view, WorkloadType> ALL_WORKLOAD_TOKENS = {
    {PodSuffix, WorkloadType::Pod},
    {DeploymentSuffix, WorkloadType::Deployment},
    {JobSuffix, WorkloadType::Job},
    {CronJobSuffix, WorkloadType::CronJob},
};

WorkloadType fromSuffix(absl::string_view suffix) {
  const auto it = ALL_WORKLOAD_TOKENS.find(suffix);
  if (it != ALL_WORKLOAD_TOKENS.end()) {
    return it->second;
  }
  return WorkloadType::Pod;
}

absl::string_view toSuffix(WorkloadType workload_type) {
  switch (workload_type) {
  case WorkloadType::Deployment:
    return DeploymentSuffix;
  case WorkloadType::CronJob:
    return CronJobSuffix;
  case WorkloadType::Job:
    return JobSuffix;
  case WorkloadType::Pod:
    return PodSuffix;
  default:
    return PodSuffix;
  }
}

absl::optional<std::string> WorkloadMetadataObject::serializeAsString() const {
  std::vector<absl::string_view> parts;
  parts.push_back(WorkloadTypeToken);
  parts.push_back("=");
  parts.push_back(toSuffix(workload_type_));
  if (!workload_name_.empty()) {
    parts.push_back(",");
    parts.push_back(WorkloadNameToken);
    parts.push_back("=");
    parts.push_back(workload_name_);
  }
  if (!instance_name_.empty()) {
    parts.push_back(",");
    parts.push_back(InstanceNameToken);
    parts.push_back("=");
    parts.push_back(instance_name_);
  }
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
  return Envoy::HashUtil::xxHash64(*serializeAsString());
}

google::protobuf::Struct convertWorkloadMetadataToStruct(const WorkloadMetadataObject& obj) {
  google::protobuf::Struct metadata;
  if (!obj.instance_name_.empty()) {
    (*metadata.mutable_fields())["NAME"].set_string_value(obj.instance_name_);
  }
  if (!obj.namespace_name_.empty()) {
    (*metadata.mutable_fields())["NAMESPACE"].set_string_value(obj.namespace_name_);
  }
  if (!obj.workload_name_.empty()) {
    (*metadata.mutable_fields())["WORKLOAD_NAME"].set_string_value(obj.workload_name_);
  }
  if (!obj.cluster_name_.empty()) {
    (*metadata.mutable_fields())["CLUSTER_ID"].set_string_value(obj.cluster_name_);
  }
  auto* labels = (*metadata.mutable_fields())["LABELS"].mutable_struct_value();
  if (!obj.canonical_name_.empty()) {
    (*labels->mutable_fields())[CanonicalNameLabel].set_string_value(obj.canonical_name_);
  }
  if (!obj.canonical_revision_.empty()) {
    (*labels->mutable_fields())[CanonicalRevisionLabel].set_string_value(obj.canonical_revision_);
  }
  if (!obj.app_name_.empty()) {
    (*labels->mutable_fields())[AppNameLabel].set_string_value(obj.app_name_);
  }
  if (!obj.app_version_.empty()) {
    (*labels->mutable_fields())[AppVersionLabel].set_string_value(obj.app_version_);
  }
  std::string owner = absl::StrCat(OwnerPrefix, obj.namespace_name_, "/",
                                   toSuffix(obj.workload_type_), "s/", obj.workload_name_);
  (*metadata.mutable_fields())["OWNER"].set_string_value(owner);
  return metadata;
}

// Convert struct to a metadata object.
std::unique_ptr<WorkloadMetadataObject>
convertStructToWorkloadMetadata(const google::protobuf::Struct& metadata) {
  absl::string_view instance, namespace_name, owner, workload, cluster, canonical_name,
      canonical_revision, app_name, app_version;
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
        } else if (labels_it.first == AppNameLabel) {
          app_name = labels_it.second.string_value();
        } else if (labels_it.first == AppVersionLabel) {
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
      workload_type = fromSuffix(owner.substr(last + 1));
    }
  }

  return std::make_unique<WorkloadMetadataObject>(instance, cluster, namespace_name, workload,
                                                  canonical_name, canonical_revision, app_name,
                                                  app_version, workload_type, "");
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

std::string serializeToStringDeterministic(const google::protobuf::Struct& metadata) {
  std::string out;
  {
    google::protobuf::io::StringOutputStream md(&out);
    google::protobuf::io::CodedOutputStream mcs(&md);
    mcs.SetSerializationDeterministic(true);
    if (!metadata.SerializeToCodedStream(&mcs)) {
      out.clear();
    }
  }
  return out;
}

class WorkloadMetadataObjectReflection : public Envoy::StreamInfo::FilterState::ObjectReflection {
public:
  WorkloadMetadataObjectReflection(const WorkloadMetadataObject* object) : object_(object) {}
  FieldType getField(absl::string_view field_name) const override {
    const auto it = ALL_BAGGAGE_TOKENS.find(field_name);
    if (it != ALL_BAGGAGE_TOKENS.end()) {
      switch (it->second) {
      case BaggageToken::NamespaceName:
        return object_->namespace_name_;
      case BaggageToken::ClusterName:
        return object_->cluster_name_;
      case BaggageToken::ServiceName:
        return object_->canonical_name_;
      case BaggageToken::ServiceVersion:
        return object_->canonical_revision_;
      case BaggageToken::AppName:
        return object_->app_name_;
      case BaggageToken::AppVersion:
        return object_->app_version_;
      case BaggageToken::WorkloadName:
        return object_->workload_name_;
      case BaggageToken::WorkloadType:
        return toSuffix(object_->workload_type_);
      case BaggageToken::InstanceName:
        return object_->instance_name_;
      }
    }
    return {};
  }

private:
  const WorkloadMetadataObject* object_;
};

std::unique_ptr<WorkloadMetadataObject> convertBaggageToWorkloadMetadata(absl::string_view data) {
  absl::string_view instance;
  absl::string_view cluster;
  absl::string_view workload;
  absl::string_view namespace_name;
  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  absl::string_view app_name;
  absl::string_view app_version;
  WorkloadType workload_type = WorkloadType::Pod;
  std::vector<absl::string_view> properties = absl::StrSplit(data, ',');
  for (absl::string_view property : properties) {
    std::pair<absl::string_view, absl::string_view> parts = absl::StrSplit(property, '=');
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
      case BaggageToken::AppName:
        app_name = parts.second;
        break;
      case BaggageToken::AppVersion:
        app_version = parts.second;
        break;
      case BaggageToken::WorkloadName:
        workload = parts.second;
        break;
      case BaggageToken::WorkloadType:
        workload_type = fromSuffix(parts.second);
        break;
      case BaggageToken::InstanceName:
        instance = parts.second;
        break;
      }
    }
  }
  return std::make_unique<WorkloadMetadataObject>(instance, cluster, namespace_name, workload,
                                                  canonical_name, canonical_revision, app_name,
                                                  app_version, workload_type, "");
}

std::unique_ptr<Envoy::StreamInfo::FilterState::Object>
WorkloadMetadataObjectFactory::createFromBytes(absl::string_view data) const {
  return convertBaggageToWorkloadMetadata(data);
}

std::unique_ptr<Envoy::StreamInfo::FilterState::ObjectReflection>
WorkloadMetadataObjectFactory::reflect(const Envoy::StreamInfo::FilterState::Object* data) const {
  const auto* object = dynamic_cast<const WorkloadMetadataObject*>(data);
  if (object) {
    return std::make_unique<WorkloadMetadataObjectReflection>(object);
  }
  return nullptr;
}

class DownstreamPeerObjectFactory : public WorkloadMetadataObjectFactory {
public:
  std::string name() const override { return std::string(DownstreamPeer); }
};

REGISTER_FACTORY(DownstreamPeerObjectFactory, Envoy::StreamInfo::FilterState::ObjectFactory);

class UpstreamPeerObjectFactory : public WorkloadMetadataObjectFactory {
public:
  std::string name() const override { return std::string(UpstreamPeer); }
};

REGISTER_FACTORY(UpstreamPeerObjectFactory, Envoy::StreamInfo::FilterState::ObjectFactory);

} // namespace Common
} // namespace Istio
