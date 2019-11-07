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
#ifdef NULL_PLUGIN
namespace Wasm {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf;
using namespace google::protobuf::util;
using namespace wasm::common;

constexpr absl::string_view node_metadata_json = R"###(
{
   "NAMESPACE":"test_namespace",
   "PLATFORM_METADATA":{
      "gcp_project":"test_project",
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster"
   },
   "WORKLOAD_NAME":"test_workload",
   "OWNER":"test_owner",
   "NAME":"test_pod"
}
)###";

// Test all possible metadata field.
TEST(ContextTest, extractNodeMetadata) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  NodeInfo node_info;
  Status status = extractNodeMetadata(metadata_struct, &node_info);
  EXPECT_EQ(status, Status::OK);
  EXPECT_EQ(node_info.name(), "test_pod");
  EXPECT_EQ(node_info.namespace_(), "test_namespace");
  EXPECT_EQ(node_info.owner(), "test_owner");
  EXPECT_EQ(node_info.workload_name(), "test_workload");
  auto platform_metadata = node_info.platform_metadata();
  EXPECT_EQ(platform_metadata["gcp_project"], "test_project");
  EXPECT_EQ(platform_metadata["gcp_cluster_name"], "test_cluster");
  EXPECT_EQ(platform_metadata["gcp_cluster_location"], "test_location");
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
  EXPECT_EQ(node_info.platform_metadata_size(), 0);
}

// Test missing metadata.
TEST(ContextTest, extractNodeMetadataMissingMetadata) {
  std::string node_metadata_json = R"###(
{
   "NAMESPACE":"test_namespace",
   "NAME":"test_pod"
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
  EXPECT_EQ(node_info.platform_metadata_size(), 0);
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

// Test extractNodeMetadataValue.
TEST(ContextTest, extractNodeMetadataValue) {
  google::protobuf::Struct metadata_struct;
  auto node_metadata_map = metadata_struct.mutable_fields();
  (*node_metadata_map)["EXCHANGE_KEYS"].set_string_value("NAMESPACE,LABELS");
  (*node_metadata_map)["NAMESPACE"].set_string_value("default");
  (*node_metadata_map)["LABELS"].set_string_value("{app, details}");
  google::protobuf::Struct value_struct;
  const auto status = extractNodeMetadataValue(metadata_struct, &value_struct);
  EXPECT_EQ(status, Status::OK);
  auto namespace_iter = value_struct.fields().find("NAMESPACE");
  EXPECT_TRUE(namespace_iter != value_struct.fields().end());
  EXPECT_EQ(namespace_iter->second.string_value(), "default");
  auto label_iter = value_struct.fields().find("LABELS");
  EXPECT_TRUE(label_iter != value_struct.fields().end());
  EXPECT_EQ(label_iter->second.string_value(), "{app, details}");
}

}  // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Wasm
#endif
