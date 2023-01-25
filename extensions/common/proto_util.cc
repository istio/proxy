/* Copyright 2020 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "extensions/common/proto_util.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "extensions/common/util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else // NULL_PLUGIN

#include "include/proxy-wasm/null_plugin.h"

#endif // NULL_PLUGIN

// END WASM_PROLOG

namespace Wasm {
namespace Common {

flatbuffers::DetachedBuffer
extractNodeFlatBufferFromStruct(const google::protobuf::Struct& metadata) {
  flatbuffers::FlatBufferBuilder fbb;
  flatbuffers::Offset<flatbuffers::String> name, namespace_, owner, workload_name, istio_version,
      mesh_id, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels, platform_metadata;
  std::vector<flatbuffers::Offset<flatbuffers::String>> app_containers;
  std::vector<flatbuffers::Offset<flatbuffers::String>> ip_addrs;
  for (const auto& it : metadata.fields()) {
    if (it.first == "NAME") {
      name = fbb.CreateString(it.second.string_value());
    } else if (it.first == "NAMESPACE") {
      namespace_ = fbb.CreateString(it.second.string_value());
    } else if (it.first == "OWNER") {
      owner = fbb.CreateString(it.second.string_value());
    } else if (it.first == "WORKLOAD_NAME") {
      workload_name = fbb.CreateString(it.second.string_value());
    } else if (it.first == "ISTIO_VERSION") {
      istio_version = fbb.CreateString(it.second.string_value());
    } else if (it.first == "MESH_ID") {
      mesh_id = fbb.CreateString(it.second.string_value());
    } else if (it.first == "CLUSTER_ID") {
      cluster_id = fbb.CreateString(it.second.string_value());
    } else if (it.first == "LABELS") {
      for (const auto& labels_it : it.second.struct_value().fields()) {
        labels.push_back(CreateKeyVal(fbb, fbb.CreateString(labels_it.first),
                                      fbb.CreateString(labels_it.second.string_value())));
      }
    } else if (it.first == "PLATFORM_METADATA") {
      for (const auto& platform_it : it.second.struct_value().fields()) {
        platform_metadata.push_back(
            CreateKeyVal(fbb, fbb.CreateString(platform_it.first),
                         fbb.CreateString(platform_it.second.string_value())));
      }
    } else if (it.first == "APP_CONTAINERS") {
      std::vector<absl::string_view> containers = absl::StrSplit(it.second.string_value(), ',');
      for (const auto& container : containers) {
        app_containers.push_back(fbb.CreateString(toStdStringView(container)));
      }
    } else if (it.first == "INSTANCE_IPS") {
      std::vector<absl::string_view> ip_addresses = absl::StrSplit(it.second.string_value(), ',');
      for (const auto& ip : ip_addresses) {
        ip_addrs.push_back(fbb.CreateString(toStdStringView(ip)));
      }
    }
  }
  // finish pre-order construction
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<KeyVal>>> labels_offset,
      platform_metadata_offset;
  if (labels.size() > 0) {
    labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  }
  if (platform_metadata.size() > 0) {
    platform_metadata_offset = fbb.CreateVectorOfSortedTables(&platform_metadata);
  }
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>
      app_containers_offset;
  if (app_containers.size() > 0) {
    app_containers_offset = fbb.CreateVector(app_containers);
  }
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>
      ip_addrs_offset;
  if (ip_addrs.size() > 0) {
    ip_addrs_offset = fbb.CreateVector(ip_addrs);
  }
  FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  node.add_owner(owner);
  node.add_workload_name(workload_name);
  node.add_istio_version(istio_version);
  node.add_mesh_id(mesh_id);
  node.add_cluster_id(cluster_id);
  node.add_labels(labels_offset);
  node.add_platform_metadata(platform_metadata_offset);
  if (app_containers.size() > 0) {
    node.add_app_containers(app_containers_offset);
  }
  if (ip_addrs.size() > 0) {
    node.add_instance_ips(ip_addrs_offset);
  }
  auto data = node.Finish();
  fbb.Finish(data);
  return fbb.Release();
}

void extractStructFromNodeFlatBuffer(const FlatNode& node, google::protobuf::Struct* metadata) {
  if (node.name()) {
    (*metadata->mutable_fields())["NAME"].set_string_value(node.name()->str());
  }
  if (node.namespace_()) {
    (*metadata->mutable_fields())["NAMESPACE"].set_string_value(node.namespace_()->str());
  }
  if (node.owner()) {
    (*metadata->mutable_fields())["OWNER"].set_string_value(node.owner()->str());
  }
  if (node.workload_name()) {
    (*metadata->mutable_fields())["WORKLOAD_NAME"].set_string_value(node.workload_name()->str());
  }
  if (node.istio_version()) {
    (*metadata->mutable_fields())["ISTIO_VERSION"].set_string_value(node.istio_version()->str());
  }
  if (node.mesh_id()) {
    (*metadata->mutable_fields())["MESH_ID"].set_string_value(node.mesh_id()->str());
  }
  if (node.cluster_id()) {
    (*metadata->mutable_fields())["CLUSTER_ID"].set_string_value(node.cluster_id()->str());
  }
  if (node.labels()) {
    auto* map = (*metadata->mutable_fields())["LABELS"].mutable_struct_value();
    for (const auto keyval : *node.labels()) {
      (*map->mutable_fields())[flatbuffers::GetString(keyval->key())].set_string_value(
          flatbuffers::GetString(keyval->value()));
    }
  }
  if (node.platform_metadata()) {
    auto* map = (*metadata->mutable_fields())["PLATFORM_METADATA"].mutable_struct_value();
    for (const auto keyval : *node.platform_metadata()) {
      (*map->mutable_fields())[flatbuffers::GetString(keyval->key())].set_string_value(
          flatbuffers::GetString(keyval->value()));
    }
  }
  if (node.app_containers()) {
    std::vector<std::string> containers;
    for (const auto container : *node.app_containers()) {
      containers.push_back(flatbuffers::GetString(container));
    }
    (*metadata->mutable_fields())["APP_CONTAINERS"].set_string_value(
        absl::StrJoin(containers, ","));
  }
  if (node.instance_ips()) {
    std::vector<std::string> ip_addrs;
    for (const auto ip : *node.instance_ips()) {
      ip_addrs.push_back(flatbuffers::GetString(ip));
    }
    (*metadata->mutable_fields())["INSTANCE_IPS"].set_string_value(absl::StrJoin(ip_addrs, ","));
  }
}

bool serializeToStringDeterministic(const google::protobuf::Message& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    return false;
  }
  return true;
}

} // namespace Common
} // namespace Wasm
