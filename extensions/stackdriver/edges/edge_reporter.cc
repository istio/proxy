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

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/edges/edges.pb.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else
#include "extensions/common/wasm/null/null_plugin.h"
#endif

namespace Extensions {
namespace Stackdriver {
namespace Edges {

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;
using google::cloud::meshtelemetry::v1alpha1::TrafficAssertion;
using google::cloud::meshtelemetry::v1alpha1::
    TrafficAssertion_Protocol_PROTOCOL_GRPC;
using google::cloud::meshtelemetry::v1alpha1::
    TrafficAssertion_Protocol_PROTOCOL_HTTP;
using google::cloud::meshtelemetry::v1alpha1::
    TrafficAssertion_Protocol_PROTOCOL_HTTPS;
using google::cloud::meshtelemetry::v1alpha1::
    TrafficAssertion_Protocol_PROTOCOL_TCP;
using google::cloud::meshtelemetry::v1alpha1::WorkloadInstance;

constexpr char kUnknown[] = "unknown";

std::string valueOrUnknown(const std::string& value) {
  if (value.length() == 0) {
    return kUnknown;
  }
  return value;
}

namespace {
void instanceFromMetadata(const ::wasm::common::NodeInfo& node_info,
                          WorkloadInstance* instance) {
  // TODO(douglas-reid): support more than just kubernetes instances
  absl::StrAppend(instance->mutable_uid(), "kubernetes://",
                  valueOrUnknown(node_info.name()), ".",
                  valueOrUnknown(node_info.namespace_()));
  // TODO(douglas-reid): support more than just GCP ?
  const auto& platform_metadata = node_info.platform_metadata();
  const auto location_iter = platform_metadata.find(Common::kGCPLocationKey);
  if (location_iter != platform_metadata.end()) {
    instance->set_location(location_iter->second);
  } else {
    instance->set_location(kUnknown);
  }
  const auto cluster_iter = platform_metadata.find(Common::kGCPClusterNameKey);
  if (cluster_iter != platform_metadata.end()) {
    instance->set_cluster_name(cluster_iter->second);
  } else {
    instance->set_cluster_name(kUnknown);
  }
  instance->set_owner_uid(valueOrUnknown(node_info.owner()));
  instance->set_workload_name(valueOrUnknown(node_info.workload_name()));
  instance->set_workload_namespace(valueOrUnknown(node_info.namespace_()));
};

}  // namespace

EdgeReporter::EdgeReporter(const ::wasm::common::NodeInfo& local_node_info,
                           std::unique_ptr<MeshEdgesServiceClient> edges_client)
    : EdgeReporter(local_node_info, std::move(edges_client), []() {
        return TimeUtil::NanosecondsToTimestamp(getCurrentTimeNanoseconds());
      }) {}

EdgeReporter::EdgeReporter(const ::wasm::common::NodeInfo& local_node_info,
                           std::unique_ptr<MeshEdgesServiceClient> edges_client,
                           TimestampFn now)
    : edges_client_(std::move(edges_client)), now_(now) {
  current_request_ = std::make_unique<ReportTrafficAssertionsRequest>();

  const auto iter =
      local_node_info.platform_metadata().find(Common::kGCPProjectKey);
  if (iter != local_node_info.platform_metadata().end()) {
    current_request_->set_parent("projects/" + iter->second);
  }

  std::string mesh_id = local_node_info.mesh_id();
  if (mesh_id.empty()) {
    mesh_id = "unknown";
  }
  current_request_->set_mesh_uid(mesh_id);

  instanceFromMetadata(local_node_info, &node_instance_);
};

EdgeReporter::~EdgeReporter() {
  // if (current_request_->traffic_assertions_size() == 0 ||
  // !queued_requests_.empty()) {
  //   logWarn("EdgeReporter had uncommitted TrafficAssertions when shutdown.");
  // }
}

// ONLY inbound
void EdgeReporter::addEdge(const ::Wasm::Common::RequestInfo& request_info,
                           const std::string& peer_metadata_id_key,
                           const ::wasm::common::NodeInfo& peer_node_info) {
  const auto& peer = current_peers_.emplace(peer_metadata_id_key);
  if (!peer.second) {
    // peer edge already exists
    return;
  }

  auto* traffic_assertions = current_request_->mutable_traffic_assertions();
  auto* edge = traffic_assertions->Add();

  edge->set_destination_service_name(request_info.destination_service_name);
  edge->set_destination_service_namespace(node_instance_.workload_namespace());
  instanceFromMetadata(peer_node_info, edge->mutable_source());
  edge->mutable_destination()->CopyFrom(node_instance_);

  auto protocol = request_info.request_protocol;
  if (protocol == "http" || protocol == "HTTP") {
    edge->set_protocol(TrafficAssertion_Protocol_PROTOCOL_HTTP);
  } else if (protocol == "https" || protocol == "HTTPS") {
    edge->set_protocol(TrafficAssertion_Protocol_PROTOCOL_HTTPS);
  } else if (protocol == "grpc" || protocol == "GRPC") {
    edge->set_protocol(TrafficAssertion_Protocol_PROTOCOL_GRPC);
  } else {
    edge->set_protocol(TrafficAssertion_Protocol_PROTOCOL_TCP);
  }

  if (current_request_->traffic_assertions_size() >
      max_assertions_per_request_) {
    reportEdges();
  }
};  // namespace Edges

void EdgeReporter::reportEdges() {
  flush();
  for (auto& req : queued_requests_) {
    edges_client_->reportTrafficAssertions(*req.get());
  }
  queued_requests_.clear();
};

void EdgeReporter::flush() {
  if (current_request_->traffic_assertions_size() == 0) {
    return;
  }

  std::unique_ptr<ReportTrafficAssertionsRequest> queued_request =
      std::make_unique<ReportTrafficAssertionsRequest>();
  queued_request->set_parent(current_request_->parent());
  queued_request->set_mesh_uid(current_request_->mesh_uid());

  current_peers_.clear();
  current_request_.swap(queued_request);

  // set the timestamp and then send the queued request
  *queued_request->mutable_timestamp() = now_();
  queued_requests_.emplace_back(std::move(queued_request));
}

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions
