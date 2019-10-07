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

#pragma once

#include <string>
#include <vector>

#include "extensions/common/context.h"
#include "extensions/stackdriver/edges/edges.pb.h"
#include "extensions/stackdriver/edges/mesh_edges_service_client.h"

namespace Extensions {
namespace Stackdriver {
namespace Edges {

#ifdef NULL_PLUGIN
using Envoy::Extensions::Common::Wasm::Null::Plugin::Extensions::Stackdriver::
    Edges::MeshEdgesServiceClient;
#endif

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;
using google::cloud::meshtelemetry::v1alpha1::WorkloadInstance;

// EdgeReporter provides a mechanism for generating information on traffic
// "edges" for a mesh. It should be used **only** to document incoming edges for
// a proxy. This means that the proxy in which this reporter is running should
// be the destination workload instance for all reported traffic.
// This should only be used in a single-threaded context. No support for
// threading is currently provided.
class EdgeReporter {
 public:
  EdgeReporter(const ::wasm::common::NodeInfo &local_node_info,
               std::unique_ptr<MeshEdgesServiceClient> edges_client);

  // addEdge creates a traffic assertion (aka an edge) based on the
  // the supplied request / peer info. The new edge is added to the
  // pending request that will be sent with all generated edges.
  void addEdge(const ::Wasm::Common::RequestInfo &request_info,
               const ::wasm::common::NodeInfo &peer_node_info);

  // reportEdges sends the buffered requests to the configured edges
  // service via the supplied client.
  void reportEdges();

 private:
  // flush pushes the current pending request (iff there have been
  // edges added) into the request buffer, and swaps in a new request
  // to gather additional edges.
  void flush();

  // represents the workload instance for the current proxy
  WorkloadInstance node_instance_;

  // the active pending request to which edges are being added
  std::unique_ptr<ReportTrafficAssertionsRequest> current_request_;

  // the buffer of pending requests, awaiting transmission to the edges service
  std::vector<std::unique_ptr<ReportTrafficAssertionsRequest>> request_queue_;

  // client used to send requests to the edges service
  std::unique_ptr<MeshEdgesServiceClient> edges_client_;

  // current peers for which edges have been created in current_request_;
  std::set<std::string> current_peers_;

  // TODO(douglas-reid): make adjustable.
  const int max_assertions_per_request_ = 1000;
};

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions
