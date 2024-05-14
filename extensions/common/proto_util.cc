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
#include "extensions/common/metadata_object.h"

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
  flatbuffers::Offset<flatbuffers::String> name, namespace_, owner, workload_name, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels, platform_metadata;
  for (const auto& it : metadata.fields()) {
    if (it.first == "NAME") {
      name = fbb.CreateString(it.second.string_value());
    } else if (it.first == "NAMESPACE") {
      namespace_ = fbb.CreateString(it.second.string_value());
    } else if (it.first == "OWNER") {
      owner = fbb.CreateString(it.second.string_value());
    } else if (it.first == "WORKLOAD_NAME") {
      workload_name = fbb.CreateString(it.second.string_value());
    } else if (it.first == "CLUSTER_ID") {
      cluster_id = fbb.CreateString(it.second.string_value());
    } else if (it.first == "LABELS") {
      for (const auto& labels_it : it.second.struct_value().fields()) {
        if (labels_it.first == Istio::Common::CanonicalNameLabel ||
            labels_it.first == Istio::Common::CanonicalRevisionLabel ||
            labels_it.first == Istio::Common::AppLabel ||
            labels_it.first == Istio::Common::VersionLabel) {
          labels.push_back(CreateKeyVal(fbb, fbb.CreateString(labels_it.first),
                                        fbb.CreateString(labels_it.second.string_value())));
        }
      }
    } else if (it.first == "PLATFORM_METADATA") {
      for (const auto& platform_it : it.second.struct_value().fields()) {
        platform_metadata.push_back(
            CreateKeyVal(fbb, fbb.CreateString(platform_it.first),
                         fbb.CreateString(platform_it.second.string_value())));
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
  FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  node.add_owner(owner);
  node.add_workload_name(workload_name);
  node.add_cluster_id(cluster_id);
  node.add_labels(labels_offset);
  node.add_platform_metadata(platform_metadata_offset);
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
