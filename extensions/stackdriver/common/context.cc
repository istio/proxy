/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "extensions/stackdriver/common/context.h"
#include "extensions/stackdriver/common/constants.h"

namespace Extensions {
namespace Stackdriver {
namespace Common {

// Find value of the given key from the metadata proto map and copy it into the
// field. If the given key cannot be found, field remains unset.
void FillNodeMetadataField(
    const google::protobuf::Map<std::string, google::protobuf::Value> &metadata,
    const std::string &key, std::string *field) {
  auto iter = metadata.find(key);
  if (iter == metadata.end()) {
    return;
  }
  *field = iter->second.string_value();
}

// Same as above, but instead of finding a string value, find a string map value
// and copy it into the given field.
void FillNodeMetadataField(
    const google::protobuf::Map<std::string, google::protobuf::Value> &metadata,
    const std::string &key,
    std::unordered_map<std::string, std::string> *map_field) {
  auto iter = metadata.find(key);
  if (iter == metadata.end()) {
    return;
  }
  if (iter->second.kind_case() != google::protobuf::Value::kStructValue) {
    // expect the value to be a struct proto.
    return;
  }
  auto m = iter->second.struct_value().fields();
  for (const auto &elem : m) {
    if (elem.second.kind_case() != google::protobuf::Value::kStringValue) {
      // The map should be a string map. Skip the elem if it is not the case.
      continue;
    }
    (*map_field)[elem.first] = elem.second.string_value();
  }
}

void ExtractNodeMetadata(const google::protobuf::Struct &metadata,
                         NodeInfo *node_info) {
  const auto &istio_metadata_fields = metadata.fields();
  if (istio_metadata_fields.empty()) {
    return;
  }
  FillNodeMetadataField(istio_metadata_fields, kMetadataPodNameKey,
                        &node_info->name);
  FillNodeMetadataField(istio_metadata_fields, kMetadataNamespaceKey,
                        &node_info->namespace_name);
  FillNodeMetadataField(istio_metadata_fields, kMetadataOwnerKey,
                        &node_info->owner);
  FillNodeMetadataField(istio_metadata_fields, kMetadataWorkloadNameKey,
                        &node_info->workload_name);
  FillNodeMetadataField(istio_metadata_fields, kMetadataContainersKey,
                        &node_info->port_to_container);

  // Fill GCP project metadata
  auto iter = istio_metadata_fields.find(kPlatformMetadataKey);
  if (iter == istio_metadata_fields.end()) {
    return;
  }
  const auto &platform_metadata_struct = iter->second.struct_value();
  if (platform_metadata_struct.fields().empty()) {
    return;
  }
  const auto &platform_metadata_fields = platform_metadata_struct.fields();
  FillNodeMetadataField(platform_metadata_fields, kGCPProjectKey,
                        &node_info->project_id);
  FillNodeMetadataField(platform_metadata_fields, kGCPClusterLocationKey,
                        &node_info->location);
  FillNodeMetadataField(platform_metadata_fields, kGCPClusterNameKey,
                        &node_info->cluster_name);

  return;
}

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions
