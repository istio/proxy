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

namespace {
void instanceFromMetadata(const ::Wasm::Common::FlatNode& node_info,
                          WorkloadInstance* instance) {
  // TODO(douglas-reid): support more than just kubernetes instances
  auto name =
      node_info.name() ? node_info.name()->string_view() : absl::string_view();
  auto namespace_ = node_info.namespace_()
                        ? node_info.namespace_()->string_view()
                        : absl::string_view();

  if (Common::isRawGCEInstance(node_info)) {
    instance->set_uid(Common::getGCEInstanceUID(node_info));
  } else if (name.size() > 0 && namespace_.size() > 0) {
    absl::StrAppend(instance->mutable_uid(), "kubernetes://", name, ".",
                    namespace_);
  }

  // TODO(douglas-reid): support more than just GCP ?
  const auto platform_metadata = node_info.platform_metadata();
  if (platform_metadata) {
    const auto location_iter =
        platform_metadata->LookupByKey(Common::kGCPLocationKey);
    if (location_iter) {
      instance->set_location(flatbuffers::GetString(location_iter->value()));
    }
    const auto cluster_iter =
        platform_metadata->LookupByKey(Common::kGCPClusterNameKey);
    if (cluster_iter) {
      instance->set_cluster_name(flatbuffers::GetString(cluster_iter->value()));
    }
  }

  instance->set_owner_uid(Common::getOwner(node_info));
  instance->set_workload_name(
      flatbuffers::GetString(node_info.workload_name()));
  instance->set_workload_namespace(
      flatbuffers::GetString(node_info.namespace_()));

  const auto labels = node_info.labels();
  if (labels) {
    const auto svc_iter =
        labels->LookupByKey(Wasm::Common::kCanonicalServiceLabelName.data());
    if (svc_iter) {
      instance->set_canonical_service(
          flatbuffers::GetString(svc_iter->value()));
    }
    const auto rev_iter = labels->LookupByKey(
        Wasm::Common::kCanonicalServiceRevisionLabelName.data());
    if (rev_iter) {
      instance->set_canonical_revision(
          flatbuffers::GetString(rev_iter->value()));
    }
  }
};

}  // namespace

EdgeReporter::EdgeReporter(const ::Wasm::Common::FlatNode& local_node_info,
                           std::unique_ptr<MeshEdgesServiceClient> edges_client,
                           int batch_size)
    : EdgeReporter(local_node_info, std::move(edges_client), batch_size, []() {
        return TimeUtil::NanosecondsToTimestamp(getCurrentTimeNanoseconds());
      }) {}

EdgeReporter::EdgeReporter(const ::Wasm::Common::FlatNode& local_node_info,
                           std::unique_ptr<MeshEdgesServiceClient> edges_client,
                           int batch_size, TimestampFn now)
    : edges_client_(std::move(edges_client)),
      now_(now),
      max_assertions_per_request_(batch_size) {
  current_request_ = std::make_unique<ReportTrafficAssertionsRequest>();
  epoch_current_request_ = std::make_unique<ReportTrafficAssertionsRequest>();

  const auto platform_metadata = local_node_info.platform_metadata();
  if (platform_metadata) {
    const auto iter = platform_metadata->LookupByKey(Common::kGCPProjectKey);
    if (iter) {
      current_request_->set_parent("projects/" +
                                   flatbuffers::GetString(iter->value()));
      epoch_current_request_->set_parent("projects/" +
                                         flatbuffers::GetString(iter->value()));
    }
  }

  std::string mesh_id = flatbuffers::GetString(local_node_info.mesh_id());
  if (mesh_id.empty()) {
    mesh_id = "unknown";
  }
  current_request_->set_mesh_uid(mesh_id);
  epoch_current_request_->set_mesh_uid(mesh_id);

  instanceFromMetadata(local_node_info, &node_instance_);
};

EdgeReporter::~EdgeReporter() {}

// ONLY inbound
void EdgeReporter::addEdge(const ::Wasm::Common::RequestInfo& request_info,
                           const std::string& peer_metadata_id_key,
                           const ::Wasm::Common::FlatNode& peer_node_info) {
  const auto& peer = known_peers_.emplace(peer_metadata_id_key);
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

  auto* epoch_assertion =
      epoch_current_request_->mutable_traffic_assertions()->Add();
  epoch_assertion->MergeFrom(*edge);

  if (current_request_->traffic_assertions_size() >
      max_assertions_per_request_) {
    rotateCurrentRequest();
  }

  if (epoch_current_request_->traffic_assertions_size() >
      max_assertions_per_request_) {
    rotateEpochRequest();
  }

};  // namespace Edges

void EdgeReporter::reportEdges(bool full_epoch) {
  flush(full_epoch);
  auto timestamp = now_();
  if (full_epoch) {
    for (auto& req : epoch_queued_requests_) {
      // update all assertions
      auto assertion = req.get();
      *assertion->mutable_timestamp() = timestamp;
      edges_client_->reportTrafficAssertions(*assertion);
    }
    epoch_queued_requests_.clear();
    current_queued_requests_.clear();
  } else {
    for (auto& req : current_queued_requests_) {
      auto assertion = req.get();
      *assertion->mutable_timestamp() = timestamp;
      edges_client_->reportTrafficAssertions(*assertion);
    }
    current_queued_requests_.clear();
  }
};

void EdgeReporter::flush(bool flush_epoch) {
  rotateCurrentRequest();
  if (flush_epoch) {
    rotateEpochRequest();
    known_peers_.clear();
  }
}

void EdgeReporter::rotateCurrentRequest() {
  if (current_request_->traffic_assertions_size() == 0) {
    return;
  }
  std::unique_ptr<ReportTrafficAssertionsRequest> queued_request =
      std::make_unique<ReportTrafficAssertionsRequest>();
  queued_request->set_parent(current_request_->parent());
  queued_request->set_mesh_uid(current_request_->mesh_uid());
  current_request_.swap(queued_request);
  current_queued_requests_.emplace_back(std::move(queued_request));
}

void EdgeReporter::rotateEpochRequest() {
  if (epoch_current_request_->traffic_assertions_size() == 0) {
    return;
  }
  std::unique_ptr<ReportTrafficAssertionsRequest> queued_request =
      std::make_unique<ReportTrafficAssertionsRequest>();
  queued_request->set_parent(epoch_current_request_->parent());
  queued_request->set_mesh_uid(epoch_current_request_->mesh_uid());
  epoch_current_request_.swap(queued_request);
  epoch_queued_requests_.emplace_back(std::move(queued_request));
}

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions
