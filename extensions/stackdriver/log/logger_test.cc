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

#include "extensions/stackdriver/log/logger.h"

#include <memory>

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/common/utils.h"
#include "gmock/gmock.h"
#include "google/logging/v2/log_entry.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Log {

using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::TimeUtil;

constexpr char kServerAccessLogName[] =
    "projects/test_project/logs/server-accesslog-stackdriver";

namespace {

class MockExporter : public Exporter {
 public:
  MOCK_CONST_METHOD1(
      exportLogs,
      void(const std::vector<std::unique_ptr<
               const google::logging::v2::WriteLogEntriesRequest>>&));
};

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
  node_info.set_workload_name("test_workload");
  node_info.set_mesh_id("mesh");
  return node_info;
}

wasm::common::NodeInfo peerNodeInfo() {
  wasm::common::NodeInfo node_info;
  (*node_info.mutable_platform_metadata())[Common::kGCPProjectKey] =
      "test_project";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterNameKey] =
      "test_cluster";
  (*node_info.mutable_platform_metadata())[Common::kGCPLocationKey] =
      "test_location";
  node_info.set_namespace_("test_peer_namespace");
  node_info.set_name("test_peer_pod");
  return node_info;
}

::Wasm::Common::RequestInfo requestInfo() {
  ::Wasm::Common::RequestInfo request_info;
  request_info.start_timestamp = 0;
  request_info.request_operation = "GET";
  request_info.destination_service_host = "httpbin.org";
  request_info.response_flag = "-";
  request_info.request_protocol = "HTTP";
  request_info.destination_principal = "destination_principal";
  request_info.source_principal = "source_principal";
  request_info.service_auth_policy =
      ::Wasm::Common::ServiceAuthenticationPolicy::MutualTLS;
  return request_info;
}

google::logging::v2::WriteLogEntriesRequest expectedRequest(
    int log_entry_count) {
  auto request_info = requestInfo();
  auto peer_node_info = peerNodeInfo();
  auto node_info = nodeInfo();
  google::logging::v2::WriteLogEntriesRequest req;
  req.set_log_name(kServerAccessLogName);
  google::api::MonitoredResource monitored_resource;
  Common::getMonitoredResource(Common::kContainerMonitoredResource, node_info,
                               &monitored_resource);
  req.mutable_resource()->CopyFrom(monitored_resource);
  auto top_label_map = req.mutable_labels();
  (*top_label_map)["destination_name"] = node_info.name();
  (*top_label_map)["destination_workload"] = node_info.workload_name();
  (*top_label_map)["destination_namespace"] = node_info.namespace_();
  (*top_label_map)["mesh_uid"] = node_info.mesh_id();
  for (int i = 0; i < log_entry_count; i++) {
    auto* new_entry = req.mutable_entries()->Add();
    *new_entry->mutable_timestamp() = TimeUtil::SecondsToTimestamp(0);
    new_entry->set_severity(::google::logging::type::INFO);
    auto label_map = new_entry->mutable_labels();
    (*label_map)["source_name"] = peer_node_info.name();
    (*label_map)["source_workload"] = peer_node_info.workload_name();
    (*label_map)["source_namespace"] = peer_node_info.namespace_();

    (*label_map)["request_operation"] = request_info.request_operation;
    (*label_map)["destination_service_host"] =
        request_info.destination_service_host;
    (*label_map)["response_flag"] = request_info.response_flag;
    (*label_map)["protocol"] = request_info.request_protocol;
    (*label_map)["destination_principal"] = request_info.destination_principal;
    (*label_map)["source_principal"] = request_info.source_principal;
    (*label_map)["service_authentication_policy"] =
        std::string(::Wasm::Common::AuthenticationPolicyString(
            request_info.service_auth_policy));
  }
  return req;
}

}  // namespace

TEST(LoggerTest, TestWriteLogEntry) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  auto logger = std::make_unique<Logger>(nodeInfo(), std::move(exporter));
  logger->addLogEntry(requestInfo(), peerNodeInfo());
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<
                 const google::logging::v2::WriteLogEntriesRequest>>&
                 requests) {
            auto expected_request = expectedRequest(1);
            for (const auto& req : requests) {
              EXPECT_TRUE(MessageDifferencer::Equals(expected_request, *req));
            }
          }));
  logger->exportLogEntry();
}

TEST(LoggerTest, TestWriteLogEntryRotation) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  auto logger = std::make_unique<Logger>(nodeInfo(), std::move(exporter), 900);
  for (int i = 0; i < 9; i++) {
    logger->addLogEntry(requestInfo(), peerNodeInfo());
  }
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<
                 const google::logging::v2::WriteLogEntriesRequest>>&
                 requests) {
            EXPECT_EQ(requests.size(), 3);
            for (const auto& req : requests) {
              auto expected_request = expectedRequest(3);
              EXPECT_TRUE(MessageDifferencer::Equals(expected_request, *req));
            }
          }));
  logger->exportLogEntry();
}

}  // namespace Log
}  // namespace Stackdriver
}  // namespace Extensions
