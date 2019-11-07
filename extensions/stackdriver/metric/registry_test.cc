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

wasm::common::NodeInfo nodeInfo() {
  wasm::common::NodeInfo node_info;
  (*node_info.mutable_platform_metadata())[Common::kGCPProjectKey] =
      "test_project";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterNameKey] =
      "test_cluster";
  (*node_info.mutable_platform_metadata())[Common::kGCPLocationKey] =
      "test_location";
  node_info.set_namespace_("test_namespace");
  node_info.set_name("test_pod");
  return node_info;
}

google::api::MonitoredResource serverMonitoredResource() {
  google::api::MonitoredResource monitored_resource;
  monitored_resource.set_type(Common::kContainerMonitoredResource);
  (*monitored_resource.mutable_labels())[Common::kProjectIDLabel] =
      "test_project";
  (*monitored_resource.mutable_labels())[Common::kLocationLabel] =
      "test_location";
  (*monitored_resource.mutable_labels())[Common::kClusterNameLabel] =
      "test_cluster";
  (*monitored_resource.mutable_labels())[Common::kNamespaceNameLabel] =
      "test_namespace";
  (*monitored_resource.mutable_labels())[Common::kPodNameLabel] = "test_pod";
  (*monitored_resource.mutable_labels())[Common::kContainerNameLabel] =
      "istio-proxy";
  return monitored_resource;
}

google::api::MonitoredResource clientMonitoredResource() {
  google::api::MonitoredResource monitored_resource;
  monitored_resource.set_type(Common::kPodMonitoredResource);
  (*monitored_resource.mutable_labels())[Common::kProjectIDLabel] =
      "test_project";
  (*monitored_resource.mutable_labels())[Common::kLocationLabel] =
      "test_location";
  (*monitored_resource.mutable_labels())[Common::kClusterNameLabel] =
      "test_cluster";
  (*monitored_resource.mutable_labels())[Common::kNamespaceNameLabel] =
      "test_namespace";
  (*monitored_resource.mutable_labels())[Common::kPodNameLabel] = "test_pod";
  return monitored_resource;
}

TEST(RegistryTest, getStackdriverOptionsProjectID) {
  wasm::common::NodeInfo node_info;
  (*node_info.mutable_platform_metadata())[Common::kGCPProjectKey] =
      "test_project";
  auto options = getStackdriverOptions(node_info);
  EXPECT_EQ(options.project_id, "test_project");
}

TEST(RegistryTest, getStackdriverOptionsMonitoredResource) {
  auto node_info = nodeInfo();
  auto expected_server_monitored_resource = serverMonitoredResource();
  auto expected_client_monitored_resource = clientMonitoredResource();

  auto options = getStackdriverOptions(node_info);
  EXPECT_EQ(options.project_id, "test_project");
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerRequestCountView),
      expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kServerRequestBytesView),
      expected_server_monitored_resource));
  EXPECT_TRUE(
      MessageDifferencer::Equals(options.per_metric_monitored_resource.at(
                                     Common::kServerResponseLatenciesView),
                                 expected_server_monitored_resource));
  EXPECT_TRUE(
      MessageDifferencer::Equals(options.per_metric_monitored_resource.at(
                                     Common::kServerResponseBytesView),
                                 expected_server_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientRequestCountView),
      expected_client_monitored_resource));
  EXPECT_TRUE(MessageDifferencer::Equals(
      options.per_metric_monitored_resource.at(Common::kClientRequestBytesView),
      expected_client_monitored_resource));
  EXPECT_TRUE(
      MessageDifferencer::Equals(options.per_metric_monitored_resource.at(
                                     Common::kClientResponseBytesView),
                                 expected_client_monitored_resource));
  EXPECT_TRUE(
      MessageDifferencer::Equals(options.per_metric_monitored_resource.at(
                                     Common::kClientRoundtripLatenciesView),
                                 expected_client_monitored_resource));
}

}  // namespace Metric
}  // namespace Stackdriver
}  // namespace Extensions
