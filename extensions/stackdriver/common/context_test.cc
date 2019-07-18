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
#include "google/protobuf/struct.pb.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Common {

// Test ExtraceNodeMetadata with all possible metadata field.
TEST(ContextTest, ExtractNodeMetadata) {
  google::protobuf::Struct metadata_struct;
  auto metadata_fields = metadata_struct.mutable_fields();
  google::protobuf::Value pod_name;
  *pod_name.mutable_string_value() = "test_pod";
  (*metadata_fields)[kMetadataPodNameKey] = pod_name;
  google::protobuf::Value namespace_name;
  *namespace_name.mutable_string_value() = "test_namespace";
  (*metadata_fields)[kMetadataNamespaceKey] = namespace_name;
  google::protobuf::Value workload_name;
  *workload_name.mutable_string_value() = "test_workload";
  (*metadata_fields)[kMetadataWorkloadNameKey] = workload_name;
  google::protobuf::Value owner;
  *owner.mutable_string_value() = "test_owner";
  (*metadata_fields)[kMetadataOwnerKey] = owner;
  google::protobuf::Value project_id;
  *project_id.mutable_string_value() = "test_project";
  auto *gcp_fields = (*metadata_fields)[kPlatformMetadataKey]
                         .mutable_struct_value()
                         ->mutable_fields();
  (*gcp_fields)[kGCPProjectKey] = project_id;
  google::protobuf::Value cluster_location;
  *cluster_location.mutable_string_value() = "test_location";
  (*gcp_fields)[kGCPClusterLocationKey] = cluster_location;
  google::protobuf::Value cluster_name;
  *cluster_name.mutable_string_value() = "test_cluster";
  (*gcp_fields)[kGCPClusterNameKey] = cluster_name;
  google::protobuf::Value container_name;
  *container_name.mutable_string_value() = "test_container";
  auto *container_map =
      (*metadata_fields)[kMetadataContainersKey].mutable_struct_value();
  (*container_map->mutable_fields())["80"] = container_name;

  NodeInfo node_info;
  ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(node_info.name, "test_pod");
  EXPECT_EQ(node_info.namespace_name, "test_namespace");
  EXPECT_EQ(node_info.owner, "test_owner");
  EXPECT_EQ(node_info.workload_name, "test_workload");
  EXPECT_EQ(node_info.project_id, "test_project");
  EXPECT_EQ(node_info.cluster_name, "test_cluster");
  EXPECT_EQ(node_info.location, "test_location");
  EXPECT_EQ(node_info.port_to_container.size(), 1);
  EXPECT_EQ(node_info.port_to_container.at("80"), "test_container");
}

// Test empty node Istio metadata.
TEST(ContextTest, ExtractNodeMetadataNoMetadataField) {
  google::protobuf::Struct metadata_struct;
  NodeInfo node_info;

  ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(node_info.name, "");
  EXPECT_EQ(node_info.namespace_name, "");
  EXPECT_EQ(node_info.owner, "");
  EXPECT_EQ(node_info.workload_name, "");
  EXPECT_EQ(node_info.project_id, "");
  EXPECT_EQ(node_info.location, "");
  EXPECT_EQ(node_info.cluster_name, "");
  EXPECT_TRUE(node_info.port_to_container.empty());
}

// Test wrong type of GCP metadata.
TEST(ContextTest, ExtractNodeMetadataWrongGCPMetadata) {
  google::protobuf::Value metadata_string;
  *metadata_string.mutable_string_value() = "some_string_metadata";
  google::protobuf::Struct metadata_struct;
  (*metadata_struct.mutable_fields())[kPlatformMetadataKey] = metadata_string;

  NodeInfo node_info;
  ExtractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(node_info.project_id, "");
  EXPECT_EQ(node_info.location, "");
  EXPECT_EQ(node_info.cluster_name, "");
  EXPECT_TRUE(node_info.port_to_container.empty());
}

// Test missing Istio metadata fields.
TEST(ContextTest, ExtractNodeMetadataFieldNotFound) {
  google::protobuf::Struct metadata_struct;
  auto metadata_fields = metadata_struct.mutable_fields();

  google::protobuf::Value pod_name;
  *pod_name.mutable_string_value() = "test_pod";
  (*metadata_fields)[kMetadataPodNameKey] = pod_name;
  google::protobuf::Value namespace_name;
  *namespace_name.mutable_string_value() = "test_namespace";
  (*metadata_fields)[kMetadataNamespaceKey] = namespace_name;

  NodeInfo node_info;
  ExtractNodeMetadata(metadata_struct, &node_info);
  // For the missing fields, the value should just be empty string.
  EXPECT_EQ(node_info.name, "test_pod");
  EXPECT_EQ(node_info.namespace_name, "test_namespace");
  EXPECT_EQ(node_info.owner, "");
  EXPECT_EQ(node_info.workload_name, "");
  EXPECT_EQ(node_info.project_id, "");
  EXPECT_EQ(node_info.location, "");
  EXPECT_EQ(node_info.cluster_name, "");
  EXPECT_TRUE(node_info.port_to_container.empty());
}

}  // namespace Common
}  // namespace Stackdriver
}  // namespace Extensions