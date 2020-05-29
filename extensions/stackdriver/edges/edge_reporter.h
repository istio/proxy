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
#include "google/protobuf/util/time_util.h"

namespace Extensions {
namespace Stackdriver {
namespace Edges {

#ifdef NULL_PLUGIN
using proxy_wasm::null_plugin::getCurrentTimeNanoseconds;
using proxy_wasm::null_plugin::Extensions::Stackdriver::Edges::
    MeshEdgesServiceClient;

#endif

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;
using google::cloud::meshtelemetry::v1alpha1::WorkloadInstance;
using google::protobuf::util::TimeUtil;

constexpr int kDefaultAssertionBatchSize = 100;

// EdgeReporter provides a mechanism for generating information on traffic
// "edges" for a mesh. It should be used **only** to document incoming edges for
// a proxy. This means that the proxy in which this reporter is running should
// be the destination workload instance for all reported traffic.
//
// EdgeReporter tracks edges in two distinct batches. A full batch of edges for
// an entire epoch of reporting is maintained, as is a batch of new edges
// observed during intervals within that epoch. This allows continual
// incremental updating of the edges in the system with a periodic full sync of
// observed edges.
//
// This should only be used in a single-threaded context. No support for
// threading is currently provided.
class EdgeReporter {
  typedef std::function<google::protobuf::Timestamp()> TimestampFn;

 public:
  EdgeReporter(const ::Wasm::Common::FlatNode& local_node_info,
               std::unique_ptr<MeshEdgesServiceClient> edges_client,
               int batch_size);

  EdgeReporter(const ::Wasm::Common::FlatNode& local_node_info,
               std::unique_ptr<MeshEdgesServiceClient> edges_client,
               int batch_size, TimestampFn now);

  ~EdgeReporter();  // this will call `reportEdges`

  // addEdge creates a traffic assertion (aka an edge) based on the
  // the supplied request / peer info. The new edge is added to the
  // pending request that will be sent with all generated edges.
  void addEdge(const ::Wasm::Common::RequestInfo& request_info,
               const std::string& peer_metadata_id_key,
               const ::Wasm::Common::FlatNode& peer_node_info);

  // reportEdges sends the buffered requests to the configured edges
  // service via the supplied client. When full_epoch is false, only
  // the most recent *new* edges are reported. When full_epoch is true,
  // all edges observed for the entire current epoch are reported.
  void reportEdges(bool full_epoch = false);

 private:
  // builds a full request out of the current traffic assertions (edges),
  // and adds that request to a queue. when flush_epoch is true, this operation
  // is performed on the epoch-maintained assertions and the cache is cleared.
  void flush(bool flush_epoch = false);

  // moves the current request to the queue and creates a new current request
  // for new edges to be added into.
  void rotateCurrentRequest();

  // moves the current epoch request to the queue and creates a new epoch
  // request for new edges to be added into.
  void rotateEpochRequest();

  // client used to send requests to the edges service
  std::unique_ptr<MeshEdgesServiceClient> edges_client_;

  // gets the current time
  TimestampFn now_;

  // the active pending new edges request to which edges are being added
  std::unique_ptr<ReportTrafficAssertionsRequest> current_request_;

  // the active pending epoch request to which edges are being added
  std::unique_ptr<ReportTrafficAssertionsRequest> epoch_current_request_;

  // represents the workload instance for the current proxy
  WorkloadInstance node_instance_;

  // current peers for which edges have been observed in the current epoch;
  std::unordered_set<std::string> known_peers_;

  // requests waiting to be sent to backend for the intra-epoch reporting
  // interval
  std::vector<std::unique_ptr<ReportTrafficAssertionsRequest>>
      current_queued_requests_;

  // requests waiting to be sent to backend for the entire epoch
  std::vector<std::unique_ptr<ReportTrafficAssertionsRequest>>
      epoch_queued_requests_;

  const int max_assertions_per_request_;
};

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions
