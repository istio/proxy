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
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "google/protobuf/util/time_util.h"
#include "test/test_common/status_utility.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Log {

using google::protobuf::util::MessageDifferencer;
using google::protobuf::util::TimeUtil;

namespace {

class MockExporter : public Exporter {
public:
  MOCK_METHOD2(
      exportLogs,
      void(const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&,
           bool));
};

const ::Wasm::Common::FlatNode& nodeInfo(flatbuffers::FlatBufferBuilder& fbb) {
  auto name = fbb.CreateString("test_pod");
  auto namespace_ = fbb.CreateString("test_namespace");
  auto workload_name = fbb.CreateString("test_workload");
  auto mesh_id = fbb.CreateString("mesh");
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
  node.add_workload_name(workload_name);
  node.add_mesh_id(mesh_id);
  node.add_platform_metadata(platform_metadata_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fbb.GetBufferPointer());
}

const ::Wasm::Common::FlatNode& peerNodeInfo(flatbuffers::FlatBufferBuilder& fbb) {
  auto name = fbb.CreateString("test_peer_pod");
  auto namespace_ = fbb.CreateString("test_peer_namespace");
  auto workload_name = fbb.CreateString("test_peer_workload");
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
  node.add_workload_name(workload_name);
  node.add_platform_metadata(platform_metadata_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  return *flatbuffers::GetRoot<::Wasm::Common::FlatNode>(fbb.GetBufferPointer());
}

::Wasm::Common::RequestInfo requestInfo(int response_code = 200) {
  ::Wasm::Common::RequestInfo request_info;
  request_info.start_time = 0;
  request_info.response_code = response_code;
  request_info.request_operation = "GET";
  request_info.destination_service_host = "httpbin.org";
  request_info.destination_service_name = "httpbin";
  request_info.response_flag = "-";
  request_info.request_protocol = ::Wasm::Common::Protocol::HTTP;
  request_info.destination_principal = "destination_principal";
  request_info.source_principal = "source_principal";
  request_info.service_auth_policy = ::Wasm::Common::ServiceAuthenticationPolicy::MutualTLS;
  request_info.duration = 10000000000; // 10s in nanoseconds
  request_info.url_scheme = "http";
  request_info.url_host = "httpbin.org";
  request_info.url_path = "/headers";
  request_info.path = "/headers?retry=true";
  request_info.request_id = "123";
  request_info.b3_trace_id = "123abc";
  request_info.b3_span_id = "abc123";
  request_info.b3_trace_sampled = true;
  request_info.user_agent = "chrome";
  request_info.referer = "www.google.com";
  request_info.source_address = "1.1.1.1";
  request_info.destination_address = "2.2.2.2";
  request_info.connection_id = 0;
  request_info.route_name = "redirect";
  request_info.upstream_cluster = "server-inbound-cluster";
  request_info.upstream_host = "1.1.1.1:1000";
  request_info.requested_server_name = "server.com";
  request_info.x_envoy_original_dst_host = "tmp.com";
  request_info.x_envoy_original_path = "/tmp";
  return request_info;
}

std::string write_audit_request_json = R"({
  "logName":"projects/test_project/logs/server-istio-audit-log",
  "resource":{
     "type":"k8s_container",
     "labels":{
        "cluster_name":"test_cluster",
        "pod_name":"test_pod",
        "location":"test_location",
        "namespace_name":"test_namespace",
        "project_id":"test_project",
        "container_name":"istio-proxy"
     }
  },
  "labels":{
     "destination_workload":"test_workload",
     "destination_namespace":"test_namespace"
  },
  "entries":[
     {
        "httpRequest":{
           "requestMethod":"GET",
           "requestUrl":"http://httpbin.org/headers?retry=true",
           "userAgent":"chrome",
           "remoteIp":"1.1.1.1",
           "referer":"www.google.com",
           "serverIp":"2.2.2.2",
           "latency":"10s",
           "protocol":"http",
           "status":"200"
        },
        "timestamp":"1970-01-01T00:00:00Z",
        "severity":"INFO",
        "labels":{
           "destination_principal":"destination_principal",
           "destination_service_host":"httpbin.org",
           "destination_service_name":"httpbin",
           "request_id":"123",
           "source_namespace":"test_peer_namespace",
           "source_principal":"source_principal",
           "source_workload":"test_peer_workload",
        },
        "trace":"projects/test_project/traces/123abc",
        "spanId":"abc123",
        "traceSampled":true
     }
  ]
})";

std::string write_log_request_json = R"({
  "logName":"projects/test_project/logs/server-accesslog-stackdriver",
  "resource":{
     "type":"k8s_container",
     "labels":{
        "cluster_name":"test_cluster",
        "pod_name":"test_pod",
        "location":"test_location",
        "namespace_name":"test_namespace",
        "project_id":"test_project",
        "container_name":"istio-proxy"
     }
  },
  "labels":{
     "destination_workload":"test_workload",
     "mesh_uid":"mesh",
     "destination_namespace":"test_namespace",
     "destination_name":"test_pod"
  },
  "entries":[
     {
        "httpRequest":{
           "requestMethod":"GET",
           "requestUrl":"http://httpbin.org/headers?retry=true",
           "userAgent":"chrome",
           "remoteIp":"1.1.1.1",
           "referer":"www.google.com",
           "serverIp":"2.2.2.2",
           "latency":"10s",
           "protocol":"http",
           "status":"200"
        },
        "timestamp":"1970-01-01T00:00:00Z",
        "severity":"INFO",
        "labels":{
           "source_name":"test_peer_pod",
           "destination_principal":"destination_principal",
           "destination_service_host":"httpbin.org",
           "destination_service_name":"httpbin",
           "request_id":"123",
           "source_namespace":"test_peer_namespace",
           "source_principal":"source_principal",
           "service_authentication_policy":"MUTUAL_TLS",
           "source_workload":"test_peer_workload",
           "response_flag":"-",
           "protocol":"http",
           "log_sampled":"false",
           "connection_id":"0",
           "upstream_cluster": "server-inbound-cluster",
           "route_name": "redirect",
           "requested_server_name": "server.com",
           "x-envoy-original-dst-host": "tmp.com",
           "x-envoy-original-path": "/tmp",
           "upstream_host": "1.1.1.1:1000"
        },
        "trace":"projects/test_project/traces/123abc",
        "spanId":"abc123",
        "traceSampled":true
     }
  ]
})";

std::string write_error_log_request_json = R"({
  "logName":"projects/test_project/logs/server-accesslog-stackdriver",
  "resource":{
     "type":"k8s_container",
     "labels":{
        "cluster_name":"test_cluster",
        "pod_name":"test_pod",
        "location":"test_location",
        "namespace_name":"test_namespace",
        "project_id":"test_project",
        "container_name":"istio-proxy"
     }
  },
  "labels":{
     "destination_workload":"test_workload",
     "mesh_uid":"mesh",
     "destination_namespace":"test_namespace",
     "destination_name":"test_pod"
  },
  "entries":[
     {
        "httpRequest":{
           "requestMethod":"GET",
           "requestUrl":"http://httpbin.org/headers?retry=true",
           "userAgent":"chrome",
           "remoteIp":"1.1.1.1",
           "referer":"www.google.com",
           "serverIp":"2.2.2.2",
           "latency":"10s",
           "protocol":"http",
           "status":"404",
        },
        "timestamp":"1970-01-01T00:00:00Z",
        "severity":"ERROR",
        "labels":{
           "source_name":"test_peer_pod",
           "destination_principal":"destination_principal",
           "destination_service_host":"httpbin.org",
           "destination_service_name":"httpbin",
           "request_id":"123",
           "source_namespace":"test_peer_namespace",
           "source_principal":"source_principal",
           "service_authentication_policy":"MUTUAL_TLS",
           "source_workload":"test_peer_workload",
           "response_flag":"-",
           "protocol":"http",
           "log_sampled":"false",
           "connection_id":"0",
           "upstream_cluster": "server-inbound-cluster",
           "route_name": "redirect",
           "requested_server_name": "server.com",
           "x-envoy-original-dst-host": "tmp.com",
           "x-envoy-original-path": "/tmp",
           "upstream_host": "1.1.1.1:1000"
        },
        "trace":"projects/test_project/traces/123abc",
        "spanId":"abc123",
        "traceSampled":true
     }
  ]
})";

google::logging::v2::WriteLogEntriesRequest
expectedRequest(int log_entry_count, bool for_audit = false, bool use_error_log = false) {
  google::logging::v2::WriteLogEntriesRequest req;
  google::protobuf::util::JsonParseOptions options;
  std::string non_audit_log = use_error_log ? write_error_log_request_json : write_log_request_json;
  const auto status =
      JsonStringToMessage((for_audit ? write_audit_request_json : non_audit_log), &req, options);
  EXPECT_OK(status);
  for (int i = 1; i < log_entry_count; i++) {
    auto* new_entry = req.mutable_entries()->Add();
    new_entry->CopyFrom(req.entries()[0]);
  }
  return req;
}

} // namespace

TEST(LoggerTest, TestWriteLogEntry) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  flatbuffers::FlatBufferBuilder local, peer;
  std::unordered_map<std::string, std::string> extra_labels;
  auto logger = std::make_unique<Logger>(nodeInfo(local), std::move(exporter), extra_labels);
  logger->addLogEntry(requestInfo(), peerNodeInfo(peer), extra_labels, false, false);
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
                 requests,
             bool) {
            for (const auto& req : requests) {
              std::string diff;
              MessageDifferencer differ;
              differ.ReportDifferencesToString(&diff);
              if (!differ.Compare(expectedRequest(1), *req)) {
                FAIL() << "unexpected log entry " << diff << "\n";
              }
            }
          }));
  logger->exportLogEntry(/* is_on_done = */ false);
}

TEST(LoggerTest, TestWriteErrorLogEntry) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  flatbuffers::FlatBufferBuilder local, peer;
  std::unordered_map<std::string, std::string> extra_labels;
  auto logger = std::make_unique<Logger>(nodeInfo(local), std::move(exporter), extra_labels);
  logger->addLogEntry(requestInfo(404), peerNodeInfo(peer), extra_labels, false, false);
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
                 requests,
             bool) {
            for (const auto& req : requests) {
              std::string diff;
              MessageDifferencer differ;
              differ.ReportDifferencesToString(&diff);
              if (!differ.Compare(expectedRequest(1, false /* audit log */, true /* error log */),
                                  *req)) {
                FAIL() << "unexpected log entry " << diff << "\n";
              }
            }
          }));
  logger->exportLogEntry(/* is_on_done = */ false);
}

TEST(LoggerTest, TestWriteLogEntryRotation) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  flatbuffers::FlatBufferBuilder local, peer;
  std::unordered_map<std::string, std::string> extra_labels;
  auto logger = std::make_unique<Logger>(nodeInfo(local), std::move(exporter), extra_labels, 1200);

  for (int i = 0; i < 10; i++) {
    logger->addLogEntry(requestInfo(), peerNodeInfo(peer), extra_labels, false, false);
  }
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
                 requests,
             bool) {
            EXPECT_EQ(requests.size(), 5);
            for (const auto& req : requests) {
              std::string diff;
              MessageDifferencer differ;
              differ.ReportDifferencesToString(&diff);
              if (!differ.Compare(expectedRequest(2), *req)) {
                FAIL() << "unexpected log entry " << diff << "\n";
              }
            }
          }));
  logger->exportLogEntry(/* is_on_done = */ false);
}

TEST(LoggerTest, TestWriteAuditEntry) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  flatbuffers::FlatBufferBuilder local, peer;
  std::unordered_map<std::string, std::string> extra_labels;
  auto logger = std::make_unique<Logger>(nodeInfo(local), std::move(exporter), extra_labels);
  logger->addLogEntry(requestInfo(), peerNodeInfo(peer), extra_labels, false, true);
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
                 requests,
             bool) {
            for (const auto& req : requests) {
              std::string diff;
              MessageDifferencer differ;
              differ.ReportDifferencesToString(&diff);
              if (!differ.Compare(expectedRequest(1, true), *req)) {
                FAIL() << "unexpected audit entry " << diff << "\n";
              }
            }
          }));
  logger->exportLogEntry(/* is_on_done = */ false);
}

TEST(LoggerTest, TestWriteAuditAndLogEntry) {
  auto exporter = std::make_unique<::testing::NiceMock<MockExporter>>();
  auto exporter_ptr = exporter.get();
  flatbuffers::FlatBufferBuilder local, peer;
  std::unordered_map<std::string, std::string> extra_labels;
  auto logger = std::make_unique<Logger>(nodeInfo(local), std::move(exporter), extra_labels);
  for (int i = 0; i < 5; i++) {
    logger->addLogEntry(requestInfo(), peerNodeInfo(peer), extra_labels, false, false);
    logger->addLogEntry(requestInfo(), peerNodeInfo(peer), extra_labels, false, true);
  }
  EXPECT_CALL(*exporter_ptr, exportLogs(::testing::_, ::testing::_))
      .WillOnce(::testing::Invoke(
          [](const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
                 requests,
             bool) {
            bool foundAudit = false;
            bool foundLog = false;
            std::string diff;
            EXPECT_EQ(requests.size(), 2);
            for (const auto& req : requests) {
              MessageDifferencer differ;
              differ.ReportDifferencesToString(&diff);
              if (differ.Compare(expectedRequest(5, true), *req)) {
                foundAudit = true;
              }

              if (differ.Compare(expectedRequest(5, false), *req)) {
                foundLog = true;
              }
            }
            if (!(foundAudit && foundLog)) {
              FAIL() << "unexpected entries, last difference: " << diff << "\n";
            }
          }));
  logger->exportLogEntry(/* is_on_done = */ false);
}

} // namespace Log
} // namespace Stackdriver
} // namespace Extensions
