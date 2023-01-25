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

#include "extensions/stackdriver/metric/registry.h"

#include "extensions/stackdriver/common/constants.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Metric {

using google::protobuf::util::MessageDifferencer;

const ::Wasm::Common::FlatNode& nodeInfo(flatbuffers::FlatBufferBuilder& fbb) {
  auto name = fbb.CreateString("test_pod");
  auto namespace_ = fbb.CreateString("test_namespace");
  std::vector<flatbuffers::Offset<::Wasm::Common::KeyVal>> platform_metadata = {
      ::Wasm::Common::CreateKeyVal(fbb, fbb.CreateString(Common::kGCPProjectKey),
                                   fbb.CreateString("test_project")),
      ::Wasm::Common::CreateKeyVal(fbb, fbb.CreateString(Common::kGCPClusterNameKey),
                                   fbb.CreateString("test_cluster")),
      ::Wasm::Common::CreateKeyVal(fbb, fbb.CreateString(Common::kGCPLocationKey),
                                   fbb.CreateString("test_location"))};
  auto platform_metadata_offset = fbb.CreateVectorOfSortedTables(&platform_metadata);
  ::Wasm::Common::FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  node.add_platform_metadata(platform_metadata_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fbb.GetBufferPointer());
}

const ::Wasm::Common::FlatNode& nodeInfoWithNoPlatform(flatbuffers::FlatBufferBuilder& fbb) {
  auto name = fbb.CreateString("test_pod");
  auto namespace_ = fbb.CreateString("test_namespace");
  ::Wasm::Common::FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  auto data = node.Finish();
  fbb.Finish(data);
  return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fbb.GetBufferPointer());
}

google::api::MonitoredResource serverMonitoredResource() {
  google::api::MonitoredResource monitored_resource;
  monitored_resource.set_type(Common::kContainerMonitoredResource);
  (*monitored_resource.mutable_labels())[Common::kProjectIDLabel] = "test_project";
  (*monitored_resource.mutable_labels())[Common::kLocationLabel] = "test_location";
  (*monitored_resource.mutable_labels())[Common::kClusterNameLabel] = "test_cluster";
  (*monitored_resource.mutable_labels())[Common::kNamespaceNameLabel] = "test_namespace";
  (*monitored_resource.mutable_labels())[Common::kPodNameLabel] = "test_pod";
  (*monitored_resource.mutable_labels())[Common::kContainerNameLabel] = "istio-proxy";
  return monitored_resource;
}

google::api::MonitoredResource clientMonitoredResource() {
  google::api::MonitoredResource monitored_resource;
  monitored_resource.set_type(Common::kPodMonitoredResource);
  (*monitored_resource.mutable_labels())[Common::kProjectIDLabel] = "test_project";
  (*monitored_resource.mutable_labels())[Common::kLocationLabel] = "test_location";
  (*monitored_resource.mutable_labels())[Common::kClusterNameLabel] = "test_cluster";
  (*monitored_resource.mutable_labels())[Common::kNamespaceNameLabel] = "test_namespace";
  (*monitored_resource.mutable_labels())[Common::kPodNameLabel] = "test_pod";
  return monitored_resource;
}

TEST(RegistryTest, getStackdriverOptionsProjectID) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto& node_info = nodeInfo(fbb);
  ::Extensions::Stackdriver::Common::StackdriverStubOption stub_option;
  auto options = getStackdriverOptions(node_info, stub_option);
  EXPECT_EQ(options.project_id, "test_project");
}

TEST(RegistryTest, getStackdriverOptionsNoProjectID) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto& node_info = nodeInfoWithNoPlatform(fbb);
  ::Extensions::Stackdriver::Common::StackdriverStubOption stub_option;
  auto options = getStackdriverOptions(node_info, stub_option);
  EXPECT_EQ(options.project_id, "");
}

TEST(RegistryTest, getStackdriverOptionsMonitoredResource) {
  flatbuffers::FlatBufferBuilder fbb;
  const auto& node_info = nodeInfo(fbb);
  auto expected_server_monitored_resource = serverMonitoredResource();
  auto expected_client_monitored_resource = clientMonitoredResource();

  ::Extensions::Stackdriver::Common::StackdriverStubOption stub_option;
  auto options = getStackdriverOptions(node_info, stub_option);
  EXPECT_EQ(options.project_id, "test_project");
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerRequestCountView),
      expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerRequestBytesView),
      expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerResponseLatenciesView),
      expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerResponseBytesView),
      expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientRequestCountView),
      expected_client_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientRequestBytesView),
      expected_client_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientResponseBytesView),
      expected_client_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientRoundtripLatenciesView),
      expected_client_monitored_resource));
}

} // namespace Metric
} // namespace Stackdriver
} // namespace Extensions
