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

#include "extensions/common/context.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace common;

// Test all possible metadata field.
TEST(ContextTest, extractNodeMetadata) {
  std::string node_metadata_json = R"###(
{
   "namespace":"test_namespace",
   "ports_to_containers":{
      "80":"test_container"
   },
   "platform_metadata":{
      "gcp_project":"test_project",
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster"
   },
   "workload_name":"test_workload",
   "owner":"test_owner",
   "name":"test_pod"
}
)###";
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(node_metadata_json, &metadata_struct, json_parse_options);
  NodeInfo node_info;
  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "test_pod");
  EXPECT_EQ(node_info.namespace_(), "test_namespace");
  EXPECT_EQ(node_info.owner(), "test_owner");
  EXPECT_EQ(node_info.workload_name(), "test_workload");
  // EXPECT_EQ(node_info.platform_metadata().gcp_project(), "test_project");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(),
  // "test_cluster");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(),
  //         "test_location");
  EXPECT_EQ(node_info.ports_to_containers().size(), 1);
  EXPECT_EQ(node_info.ports_to_containers().at("80"), "test_container");
}

// Test empty node metadata.
TEST(ContextTest, extractNodeMetadataNoMetadataField) {
  google::protobuf::Struct metadata_struct;
  NodeInfo node_info;

  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "");
  EXPECT_EQ(node_info.namespace_(), "");
  EXPECT_EQ(node_info.owner(), "");
  EXPECT_EQ(node_info.workload_name(), "");
  auto platform_metadata = node_info.platform_metadata();
  // EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test missing metadata.
TEST(ContextTest, extractNodeMetadataMissingMetadata) {
  std::string node_metadata_json = R"###(
{
   "namespace":"test_namespace",
   "name":"test_pod"
}
)###";
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(node_metadata_json, &metadata_struct, json_parse_options);
  NodeInfo node_info;
  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "test_pod");
  EXPECT_EQ(node_info.namespace_(), "test_namespace");
  EXPECT_EQ(node_info.owner(), "");
  EXPECT_EQ(node_info.workload_name(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test wrong type of GCP metadata.
TEST(ContextTest, extractNodeMetadataWrongGCPMetadata) {
  std::string node_metadata_json = R"###(
{
   "platform_metadata":"some string",
}
)###";
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(node_metadata_json, &metadata_struct, json_parse_options);
  NodeInfo node_info;
  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_NE(status, Status::OK);
  // EXPECT_EQ(node_info.platform_metadata().gcp_project(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_name(), "");
  // EXPECT_EQ(node_info.platform_metadata().gcp_cluster_location(), "");
  EXPECT_EQ(node_info.ports_to_containers().size(), 0);
}

// Test unknown field.
TEST(ContextTest, extractNodeMetadataUnknownField) {
  std::string node_metadata_json = R"###(
{
   "some_key":"some string",
}
)###";
  google::protobuf::Struct metadata_struct;
  TextFormat::ParseFromString(node_metadata_json, &metadata_struct);
  NodeInfo node_info;
  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
}

}  // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif