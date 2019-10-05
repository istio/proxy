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

#include "extensions/stackdriver/edges/edge_reporter.h"

#include <memory>

#include "extensions/stackdriver/common/constants.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Edges {

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;
using google::protobuf::util::MessageDifferencer;

namespace {

#define EXPECT_PROTO_EQUAL(want, got, message)   \
  std::string diff;                              \
  MessageDifferencer differ;                     \
  differ.ReportDifferencesToString(&diff);       \
  bool equal = differ.Compare(want, got);        \
  if (!equal) {                                  \
    std::cerr << message << " " << diff << "\n"; \
    FAIL();                                      \
  }                                              \
  return

std::unique_ptr<ReportTrafficAssertionsRequest> last_request_;

class TestMeshEdgesServiceClient : public MeshEdgesServiceClient {
 public:
  typedef std::function<void(std::unique_ptr<ReportTrafficAssertionsRequest>)>
      TestFn;

  TestMeshEdgesServiceClient(TestFn test_func)
      : request_callback_(std::move(test_func)){};

  virtual void reportTrafficAssertions(
      std::unique_ptr<ReportTrafficAssertionsRequest> request) const override {
    request_callback_(std::move(request));
  };

 private:
  TestFn request_callback_;
};

wasm::common::NodeInfo nodeInfo() {
  wasm::common::NodeInfo node_info;
  (*node_info.mutable_platform_metadata())[Common::kGCPProjectKey] =
      "test_project";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterNameKey] =
      "test_cluster";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterLocationKey] =
      "test_location";
  node_info.set_namespace_("test_namespace");
  node_info.set_name("test_pod");
  node_info.set_workload_name("test_workload");
  node_info.set_owner("kubernetes://test_owner");
  return node_info;
}

wasm::common::NodeInfo peerNodeInfo() {
  wasm::common::NodeInfo node_info;
  (*node_info.mutable_platform_metadata())[Common::kGCPProjectKey] =
      "test_project";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterNameKey] =
      "test_cluster";
  (*node_info.mutable_platform_metadata())[Common::kGCPClusterLocationKey] =
      "test_location";
  node_info.set_namespace_("test_peer_namespace");
  node_info.set_name("test_peer_pod");
  node_info.set_workload_name("test_peer_workload");
  node_info.set_owner("kubernetes://peer_owner");
  return node_info;
}

::Wasm::Common::RequestInfo requestInfo() {
  ::Wasm::Common::RequestInfo request_info;
  request_info.destination_service_host = "httpbin.org";
  request_info.request_protocol = "HTTP";
  return request_info;
}

std::unique_ptr<ReportTrafficAssertionsRequest> want() {
  auto request_info = requestInfo();
  auto peer_node_info = peerNodeInfo();
  auto node_info = nodeInfo();

  auto req = std::make_unique<ReportTrafficAssertionsRequest>();
  req->set_parent("projects/test_project");
  req->set_mesh_uid(
      "//cloudresourcemanager.googleapis.com/projects/test_project/"
      "test_location/meshes/test_cluster");

  auto* ta = req->mutable_traffic_assertions()->Add();
  ta->set_protocol(google::cloud::meshtelemetry::v1alpha1::
                       TrafficAssertion_Protocol_PROTOCOL_HTTP);
  ta->set_destination_service_name("httpbin.org");
  ta->set_destination_service_namespace("test_namespace");

  auto* source = ta->mutable_source();
  source->set_workload_namespace("test_peer_namespace");
  source->set_workload_name("test_peer_workload");
  source->set_cluster_name("test_cluster");
  source->set_location("test_location");
  source->set_owner_uid("kubernetes://peer_owner");
  source->set_uid("kubernetes://test_peer_pod.test_peer_namespace");

  auto destination = ta->mutable_destination();
  destination->set_workload_namespace("test_namespace");
  destination->set_workload_name("test_workload");
  destination->set_cluster_name("test_cluster");
  destination->set_location("test_location");
  destination->set_owner_uid("kubernetes://test_owner");
  destination->set_uid("kubernetes://test_pod.test_namespace");

  return req;
}

}  // namespace

TEST(EdgesTest, TestAddEdge) {
  int calls = 0;
  std::unique_ptr<ReportTrafficAssertionsRequest> got;

  auto test_client = std::make_unique<TestMeshEdgesServiceClient>(
      [&calls, &got](std::unique_ptr<ReportTrafficAssertionsRequest> request) {
        calls++;
        got = std::move(request);
      });

  auto edges =
      std::make_unique<EdgeReporter>(nodeInfo(), std::move(test_client));
  edges->addEdge(requestInfo(), peerNodeInfo());
  edges->reportEdges();

  // must ensure that we used the client to report the edges
  EXPECT_EQ(1, calls);

  // ignore timestamps in proto comparisons.
  got->set_allocated_timestamp(nullptr);

  EXPECT_PROTO_EQUAL(*want().get(), *got.get(),
                     "ERROR: addEdge() produced unexpected result.");
}

TEST(EdgeReporterTest, TestRequestQueue) {
  int calls = 0;
  int num_assertions = 0;

  auto test_client = std::make_unique<TestMeshEdgesServiceClient>(
      [&calls, &num_assertions](
          std::unique_ptr<ReportTrafficAssertionsRequest> request) {
        calls++;
        num_assertions += request->traffic_assertions_size();
      });

  auto edges =
      std::make_unique<EdgeReporter>(nodeInfo(), std::move(test_client));

  // force at least three queued reqs + current (four total)
  for (int i = 0; i < 3500; i++) {
    edges->addEdge(requestInfo(), peerNodeInfo());
  }
  edges->reportEdges();

  EXPECT_EQ(4, calls);
  EXPECT_EQ(3500, num_assertions);
}

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions
