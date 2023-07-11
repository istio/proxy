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

#include "extensions/common/proto_util.h"

#include "extensions/common/node_info_generated.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"

// WASM_PROLOG
#ifdef NULL_PLUGIN
namespace Wasm {
#endif // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf;
using namespace google::protobuf::util;

constexpr std::string_view node_metadata_json = R"###(
{
   "NAME":"test_pod",
   "NAMESPACE":"test_namespace",
   "OWNER":"test_owner",
   "WORKLOAD_NAME":"test_workload",
   "ISTIO_VERSION":"1.8",
   "MESH_ID":"istio-mesh",
   "CLUSTER_ID":"test-cluster",
   "LABELS":{
      "app":"test",
      "version":"v1"
    },
   "PLATFORM_METADATA":{
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster",
      "gcp_project":"test_project"
   },
   "APP_CONTAINERS": "hello,test",
   "INSTANCE_IPS": "10.10.10.1,10.10.10.2,10.10.10.3"
}
)###";

constexpr std::string_view node_metadata_json_with_missing_lists = R"###(
{
   "NAME":"test_pod",
   "NAMESPACE":"test_namespace",
   "OWNER":"test_owner",
   "WORKLOAD_NAME":"test_workload",
   "ISTIO_VERSION":"1.8",
   "MESH_ID":"istio-mesh",
   "CLUSTER_ID":"test-cluster",
   "LABELS":{
      "app":"test",
      "version":"v1"
    },
   "PLATFORM_METADATA":{
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster",
      "gcp_project":"test_project"
   },
}
)###";

// Test all possible metadata field.
TEST(ProtoUtilTest, extractNodeMetadata) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  EXPECT_TRUE(
      JsonStringToMessage(std::string(node_metadata_json), &metadata_struct, json_parse_options)
          .ok());
  auto out = extractNodeFlatBufferFromStruct(metadata_struct);
  auto peer = flatbuffers::GetRoot<FlatNode>(out.data());
  EXPECT_EQ(peer->name()->string_view(), "test_pod");
  EXPECT_EQ(peer->namespace_()->string_view(), "test_namespace");
  EXPECT_EQ(peer->owner()->string_view(), "test_owner");
  EXPECT_EQ(peer->workload_name()->string_view(), "test_workload");
  EXPECT_EQ(peer->platform_metadata()->Get(2)->key()->string_view(), "gcp_project");
  EXPECT_EQ(peer->platform_metadata()->Get(2)->value()->string_view(), "test_project");
  EXPECT_EQ(peer->app_containers()->size(), 2);
  EXPECT_EQ(peer->instance_ips()->size(), 3);
  EXPECT_EQ(peer->cluster_id()->string_view(), "test-cluster");
}

// Test all possible metadata field.
TEST(ProtoUtilTest, extractNodeMetadataWithMissingLists) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  EXPECT_TRUE(JsonStringToMessage(std::string(node_metadata_json_with_missing_lists),
                                  &metadata_struct, json_parse_options)
                  .ok());
  auto out = extractNodeFlatBufferFromStruct(metadata_struct);
  auto peer = flatbuffers::GetRoot<FlatNode>(out.data());
  EXPECT_EQ(peer->name()->string_view(), "test_pod");
  EXPECT_EQ(peer->namespace_()->string_view(), "test_namespace");
  EXPECT_EQ(peer->owner()->string_view(), "test_owner");
  EXPECT_EQ(peer->workload_name()->string_view(), "test_workload");
  EXPECT_EQ(peer->platform_metadata()->Get(2)->key()->string_view(), "gcp_project");
  EXPECT_EQ(peer->platform_metadata()->Get(2)->value()->string_view(), "test_project");
  EXPECT_EQ(peer->app_containers(), nullptr);
  EXPECT_EQ(peer->instance_ips(), nullptr);
  EXPECT_EQ(peer->cluster_id()->string_view(), "test-cluster");
}

// Test roundtripping
TEST(ProtoUtilTest, Rountrip) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  EXPECT_TRUE(
      JsonStringToMessage(std::string(node_metadata_json), &metadata_struct, json_parse_options)
          .ok());
  auto out = extractNodeFlatBufferFromStruct(metadata_struct);
  auto peer = flatbuffers::GetRoot<FlatNode>(out.data());

  google::protobuf::Struct output_struct;
  extractStructFromNodeFlatBuffer(*peer, &output_struct);

  // Validate serialized bytes
  std::string input_bytes;
  EXPECT_TRUE(serializeToStringDeterministic(metadata_struct, &input_bytes));
  std::string output_bytes;
  EXPECT_TRUE(serializeToStringDeterministic(output_struct, &output_bytes));
  EXPECT_EQ(input_bytes, output_bytes)
      << metadata_struct.DebugString() << output_struct.DebugString();
}

// Test roundtrip for an empty struct (for corner cases)
TEST(ProtoUtilTest, RountripEmpty) {
  google::protobuf::Struct metadata_struct;
  auto out = extractNodeFlatBufferFromStruct(metadata_struct);
  auto peer = flatbuffers::GetRoot<FlatNode>(out.data());
  google::protobuf::Struct output_struct;
  extractStructFromNodeFlatBuffer(*peer, &output_struct);
  EXPECT_EQ(0, output_struct.fields().size());
}

} // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
} // namespace Wasm
#endif
