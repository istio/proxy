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
#include "source/common/protobuf/utility.h"

#include "absl/strings/str_join.h"

namespace Istio {
namespace Common {

namespace {
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

absl::optional<absl::string_view> toSuffix(WorkloadType workload_type) {
  switch (workload_type) {
  case WorkloadType::Deployment:
    return DeploymentSuffix;
  case WorkloadType::CronJob:
    return CronJobSuffix;
  case WorkloadType::Job:
    return JobSuffix;
  case WorkloadType::Pod:
    return PodSuffix;
  case WorkloadType::Unknown:
    return {};
  }
}

} // namespace

Envoy::ProtobufTypes::MessagePtr WorkloadMetadataObject::serializeAsProto() const {
  auto message = std::make_unique<Envoy::ProtobufWkt::Struct>();
  auto& fields = *message->mutable_fields();
  const auto parts = serializeAsPairs();
  for (const auto& p : parts) {
    fields[std::string(p.first)] = Envoy::ValueUtil::stringValue(std::string(p.second));
  }
  return message;
}

std::vector<std::pair<absl::string_view, absl::string_view>>
WorkloadMetadataObject::serializeAsPairs() const {
  std::vector<std::pair<absl::string_view, absl::string_view>> parts;
  const auto suffix = toSuffix(workload_type_);
  if (suffix) {
    parts.push_back({WorkloadTypeToken, *suffix});
  }
  if (!workload_name_.empty()) {
    parts.push_back({WorkloadNameToken, workload_name_});
  }
  if (!instance_name_.empty()) {
    parts.push_back({InstanceNameToken, instance_name_});
  }
  if (!cluster_name_.empty()) {
    parts.push_back({ClusterNameToken, cluster_name_});
  }
  if (!namespace_name_.empty()) {
    parts.push_back({NamespaceNameToken, namespace_name_});
  }
  if (!canonical_name_.empty()) {
    parts.push_back({ServiceNameToken, canonical_name_});
  }
  if (!canonical_revision_.empty()) {
    parts.push_back({ServiceVersionToken, canonical_revision_});
  }
  if (!app_name_.empty()) {
    parts.push_back({AppNameToken, app_name_});
  }
  if (!app_version_.empty()) {
    parts.push_back({AppVersionToken, app_version_});
  }
  return parts;
}

absl::optional<std::string> WorkloadMetadataObject::serializeAsString() const {
  const auto parts = serializeAsPairs();
  return absl::StrJoin(parts, ",", absl::PairFormatter("="));
}

absl::optional<uint64_t> WorkloadMetadataObject::hash() const {
  return Envoy::HashUtil::xxHash64(*serializeAsString());
}

absl::optional<std::string> WorkloadMetadataObject::owner() const {
  const auto suffix = toSuffix(workload_type_);
  if (suffix) {
    return absl::StrCat(OwnerPrefix, namespace_name_, "/", *suffix, "s/", workload_name_);
  }
  return {};
}

WorkloadType fromSuffix(absl::string_view suffix) {
  const auto it = ALL_WORKLOAD_TOKENS.find(suffix);
  if (it != ALL_WORKLOAD_TOKENS.end()) {
    return it->second;
  }
  return WorkloadType::Unknown;
}

WorkloadType parseOwner(absl::string_view owner, absl::string_view workload) {
  // Strip "s/workload_name" and check for workload type.
  if (owner.size() > workload.size() + 2) {
    owner.remove_suffix(workload.size() + 2);
    size_t last = owner.rfind('/');
    if (last != absl::string_view::npos) {
      return fromSuffix(owner.substr(last + 1));
    }
  }
  return WorkloadType::Unknown;
}

google::protobuf::Struct convertWorkloadMetadataToStruct(const WorkloadMetadataObject& obj) {
  google::protobuf::Struct metadata;
  if (!obj.instance_name_.empty()) {
    (*metadata.mutable_fields())[InstanceMetadataField].set_string_value(obj.instance_name_);
  }
  if (!obj.namespace_name_.empty()) {
    (*metadata.mutable_fields())[NamespaceMetadataField].set_string_value(obj.namespace_name_);
  }
  if (!obj.workload_name_.empty()) {
    (*metadata.mutable_fields())[WorkloadMetadataField].set_string_value(obj.workload_name_);
  }
  if (!obj.cluster_name_.empty()) {
    (*metadata.mutable_fields())[ClusterMetadataField].set_string_value(obj.cluster_name_);
  }
  auto* labels = (*metadata.mutable_fields())[LabelsMetadataField].mutable_struct_value();
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
  if (const auto owner = obj.owner(); owner.has_value()) {
    (*metadata.mutable_fields())[OwnerMetadataField].set_string_value(*owner);
  }
  return metadata;
}

// Convert struct to a metadata object.
std::unique_ptr<WorkloadMetadataObject>
convertStructToWorkloadMetadata(const google::protobuf::Struct& metadata) {
  absl::string_view instance, namespace_name, owner, workload, cluster, canonical_name,
      canonical_revision, app_name, app_version;
  for (const auto& it : metadata.fields()) {
    if (it.first == InstanceMetadataField) {
      instance = it.second.string_value();
    } else if (it.first == NamespaceMetadataField) {
      namespace_name = it.second.string_value();
    } else if (it.first == OwnerMetadataField) {
      owner = it.second.string_value();
    } else if (it.first == WorkloadMetadataField) {
      workload = it.second.string_value();
    } else if (it.first == ClusterMetadataField) {
      cluster = it.second.string_value();
    } else if (it.first == LabelsMetadataField) {
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
  return std::make_unique<WorkloadMetadataObject>(instance, cluster, namespace_name, workload,
                                                  canonical_name, canonical_revision, app_name,
                                                  app_version, parseOwner(owner, workload), "");
}

absl::optional<WorkloadMetadataObject>
convertEndpointMetadata(const std::string& endpoint_encoding) {
  std::vector<absl::string_view> parts = absl::StrSplit(endpoint_encoding, ';');
  if (parts.size() < 5) {
    return {};
  }
  return absl::make_optional<WorkloadMetadataObject>("", parts[4], parts[1], parts[0], parts[2],
                                                     parts[3], "", "", WorkloadType::Unknown, "");
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

WorkloadMetadataObject::FieldType
WorkloadMetadataObject::getField(absl::string_view field_name) const {
  const auto it = ALL_BAGGAGE_TOKENS.find(field_name);
  if (it != ALL_BAGGAGE_TOKENS.end()) {
    switch (it->second) {
    case BaggageToken::NamespaceName:
      return namespace_name_;
    case BaggageToken::ClusterName:
      return cluster_name_;
    case BaggageToken::ServiceName:
      return canonical_name_;
    case BaggageToken::ServiceVersion:
      return canonical_revision_;
    case BaggageToken::AppName:
      return app_name_;
    case BaggageToken::AppVersion:
      return app_version_;
    case BaggageToken::WorkloadName:
      return workload_name_;
    case BaggageToken::WorkloadType:
      if (const auto value = toSuffix(workload_type_); value.has_value()) {
        return *value;
      }
    case BaggageToken::InstanceName:
      return instance_name_;
    }
  }
  return {};
}

std::unique_ptr<WorkloadMetadataObject> convertBaggageToWorkloadMetadata(absl::string_view data) {
  absl::string_view instance;
  absl::string_view cluster;
  absl::string_view workload;
  absl::string_view namespace_name;
  absl::string_view canonical_name;
  absl::string_view canonical_revision;
  absl::string_view app_name;
  absl::string_view app_version;
  WorkloadType workload_type = WorkloadType::Unknown;
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

} // namespace Common
} // namespace Istio
