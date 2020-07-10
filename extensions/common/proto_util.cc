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

#include "absl/strings/str_split.h"
#include "extensions/common/node_info_generated.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null_plugin.h"

using Envoy::Extensions::Common::Wasm::Null::Plugin::getMessageValue;

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Wasm {
namespace Common {

bool extractNodeFlatBuffer(const google::protobuf::Struct& metadata,
                           flatbuffers::FlatBufferBuilder& fbb) {
  flatbuffers::Offset<flatbuffers::String> name, namespace_, owner,
      workload_name, istio_version, mesh_id, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels, platform_metadata;
  std::vector<flatbuffers::Offset<flatbuffers::String>> app_containers;
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
        labels.push_back(
            CreateKeyVal(fbb, fbb.CreateString(labels_it.first),
                         fbb.CreateString(labels_it.second.string_value())));
      }
    } else if (it.first == "PLATFORM_METADATA") {
      for (const auto& platform_it : it.second.struct_value().fields()) {
        platform_metadata.push_back(
            CreateKeyVal(fbb, fbb.CreateString(platform_it.first),
                         fbb.CreateString(platform_it.second.string_value())));
      }
    } else if (it.first == "APP_CONTAINERS") {
      std::vector<absl::string_view> containers =
          absl::StrSplit(it.second.string_value(), ',');
      for (const auto& container : containers) {
        app_containers.push_back(fbb.CreateString(container));
      }
    }
  }
  // finish pre-order construction
  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  auto platform_metadata_offset =
      fbb.CreateVectorOfSortedTables(&platform_metadata);
  auto app_containers_offset = fbb.CreateVector(app_containers);
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
  node.add_app_containers(app_containers_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return true;
}

bool extractLocalNodeFlatBuffer(std::string* out) {
  google::protobuf::Struct node;
  if (!getMessageValue({"node", "metadata"}, &node)) {
    return false;
  }
  flatbuffers::FlatBufferBuilder fbb;
  if (!extractNodeFlatBuffer(node, fbb)) {
    return false;
  }
  out->assign(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
              fbb.GetSize());
  return true;
}

google::protobuf::util::Status extractNodeMetadataValue(
    const google::protobuf::Struct& node_metadata,
    google::protobuf::Struct* metadata) {
  if (metadata == nullptr) {
    return google::protobuf::util::Status(
        google::protobuf::util::error::INVALID_ARGUMENT,
        "metadata provided is null");
  }
  const auto key_it = node_metadata.fields().find("EXCHANGE_KEYS");
  if (key_it == node_metadata.fields().end()) {
    return google::protobuf::util::Status(
        google::protobuf::util::error::INVALID_ARGUMENT,
        "metadata exchange key is missing");
  }

  const auto& keys_value = key_it->second;
  if (keys_value.kind_case() != google::protobuf::Value::kStringValue) {
    return google::protobuf::util::Status(
        google::protobuf::util::error::INVALID_ARGUMENT,
        "metadata exchange key is not a string");
  }

  // select keys from the metadata using the keys
  const std::set<std::string> keys =
      absl::StrSplit(keys_value.string_value(), ',', absl::SkipWhitespace());
  for (auto key : keys) {
    const auto entry_it = node_metadata.fields().find(key);
    if (entry_it == node_metadata.fields().end()) {
      continue;
    }
    (*metadata->mutable_fields())[key] = entry_it->second;
  }

  return google::protobuf::util::Status(google::protobuf::util::error::OK, "");
}

}  // namespace Common
}  // namespace Wasm
