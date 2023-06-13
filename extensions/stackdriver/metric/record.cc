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

#include "extensions/stackdriver/metric/record.h"

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/metric/registry.h"
#include "google/protobuf/util/time_util.h"

using google::protobuf::util::TimeUtil;

namespace Extensions {
namespace Stackdriver {
namespace Metric {

namespace {

using Common::unknownIfEmpty;

std::string getLocalCanonicalName(const ::Wasm::Common::FlatNode& local_node_info) {
  const auto local_labels = local_node_info.labels();

  const auto local_name_iter =
      local_labels ? local_labels->LookupByKey(Wasm::Common::kCanonicalServiceLabelName.data())
                   : nullptr;
  const auto local_canonical_name =
      local_name_iter ? local_name_iter->value() : local_node_info.workload_name();

  return flatbuffers::GetString(local_canonical_name);
}

std::string getLocalCanonicalRev(const ::Wasm::Common::FlatNode& local_node_info) {
  const auto local_labels = local_node_info.labels();

  const auto local_rev_iter =
      local_labels
          ? local_labels->LookupByKey(Wasm::Common::kCanonicalServiceRevisionLabelName.data())
          : nullptr;
  const auto local_canonical_rev = local_rev_iter ? local_rev_iter->value() : nullptr;
  return local_canonical_rev ? local_canonical_rev->str() : ::Wasm::Common::kLatest.data();
}

std::string getPeerCanonicalName(const ::Wasm::Common::FlatNode& peer_node_info) {
  const auto peer_labels = peer_node_info.labels();

  const auto peer_name_iter =
      peer_labels ? peer_labels->LookupByKey(Wasm::Common::kCanonicalServiceLabelName.data())
                  : nullptr;
  const auto peer_canonical_name =
      peer_name_iter ? peer_name_iter->value() : peer_node_info.workload_name();

  return flatbuffers::GetString(peer_canonical_name);
}

std::string getPeerCanonicalRev(const ::Wasm::Common::FlatNode& peer_node_info) {
  const auto peer_labels = peer_node_info.labels();

  const auto peer_rev_iter =
      peer_labels
          ? peer_labels->LookupByKey(Wasm::Common::kCanonicalServiceRevisionLabelName.data())
          : nullptr;
  const auto peer_canonical_rev = peer_rev_iter ? peer_rev_iter->value() : nullptr;
  return peer_canonical_rev ? peer_canonical_rev->str() : ::Wasm::Common::kLatest.data();
}

TagKeyValueList getOutboundTagMap(const ::Wasm::Common::FlatNode& local_node_info,
                                  const ::Wasm::Common::FlatNode& peer_node_info,
                                  const ::Wasm::Common::RequestInfo& request_info) {
  TagKeyValueList outboundMap = {
      {meshUIDKey(), unknownIfEmpty(flatbuffers::GetString(local_node_info.mesh_id()))},
      {requestProtocolKey(),
       unknownIfEmpty(std::string(::Wasm::Common::ProtocolString(request_info.request_protocol)))},
      {serviceAuthenticationPolicyKey(),
       unknownIfEmpty(std::string(
           ::Wasm::Common::AuthenticationPolicyString(request_info.service_auth_policy)))},
      {destinationServiceNameKey(), unknownIfEmpty(request_info.destination_service_name)},
      {destinationServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.namespace_()))},
      {destinationPortKey(), unknownIfEmpty(std::to_string(request_info.destination_port))},
      {sourcePrincipalKey(), unknownIfEmpty(request_info.source_principal)},
      {sourceWorkloadNameKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.workload_name()))},
      {sourceWorkloadNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.namespace_()))},
      {sourceOwnerKey(), unknownIfEmpty(Common::getOwner(local_node_info))},
      {destinationPrincipalKey(), unknownIfEmpty(request_info.destination_principal)},
      {destinationWorkloadNameKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.workload_name()))},
      {destinationWorkloadNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.namespace_()))},
      {destinationOwnerKey(), unknownIfEmpty(Common::getOwner(peer_node_info))},
      {destinationCanonicalServiceNameKey(), unknownIfEmpty(getPeerCanonicalName(peer_node_info))},
      {destinationCanonicalServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.namespace_()))},
      {destinationCanonicalRevisionKey(), unknownIfEmpty(getPeerCanonicalRev(peer_node_info))},
      {sourceCanonicalServiceNameKey(), unknownIfEmpty(getLocalCanonicalName(local_node_info))},
      {sourceCanonicalServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.namespace_()))},
      {sourceCanonicalRevisionKey(), unknownIfEmpty(getLocalCanonicalRev(local_node_info))},
      {proxyVersionKey(), unknownIfEmpty(flatbuffers::GetString(local_node_info.istio_version()))}};
  return outboundMap;
}

TagKeyValueList getInboundTagMap(const ::Wasm::Common::FlatNode& local_node_info,
                                 const ::Wasm::Common::FlatNode& peer_node_info,
                                 const ::Wasm::Common::RequestInfo& request_info) {
  TagKeyValueList inboundMap = {
      {meshUIDKey(), unknownIfEmpty(flatbuffers::GetString(local_node_info.mesh_id()))},
      {requestProtocolKey(),
       unknownIfEmpty(std::string(::Wasm::Common::ProtocolString(request_info.request_protocol)))},
      {serviceAuthenticationPolicyKey(),
       unknownIfEmpty(std::string(
           ::Wasm::Common::AuthenticationPolicyString(request_info.service_auth_policy)))},
      {destinationServiceNameKey(), unknownIfEmpty(request_info.destination_service_name)},
      {destinationServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.namespace_()))},
      {destinationPortKey(), unknownIfEmpty(std::to_string(request_info.destination_port))},
      {sourcePrincipalKey(), unknownIfEmpty(request_info.source_principal)},
      {sourceWorkloadNameKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.workload_name()))},
      {sourceWorkloadNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.namespace_()))},
      {sourceOwnerKey(), unknownIfEmpty(Common::getOwner(peer_node_info))},
      {destinationPrincipalKey(), unknownIfEmpty(request_info.destination_principal)},
      {destinationWorkloadNameKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.workload_name()))},
      {destinationWorkloadNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.namespace_()))},
      {destinationOwnerKey(), unknownIfEmpty(Common::getOwner(local_node_info))},
      {destinationCanonicalServiceNameKey(),
       unknownIfEmpty(getLocalCanonicalName(local_node_info))},
      {destinationCanonicalServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(local_node_info.namespace_()))},
      {destinationCanonicalRevisionKey(), unknownIfEmpty(getLocalCanonicalRev(local_node_info))},
      {sourceCanonicalServiceNameKey(), unknownIfEmpty(getPeerCanonicalName(peer_node_info))},
      {sourceCanonicalServiceNamespaceKey(),
       unknownIfEmpty(flatbuffers::GetString(peer_node_info.namespace_()))},
      {sourceCanonicalRevisionKey(), unknownIfEmpty(getPeerCanonicalRev(peer_node_info))},
      {proxyVersionKey(), unknownIfEmpty(flatbuffers::GetString(local_node_info.istio_version()))}};
  return inboundMap;
}

// See:
// https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto
uint32_t httpCodeFromGrpc(uint32_t grpc_status) {
  switch (grpc_status) {
  case 0: // OK
    return 200;
  case 1: // CANCELLED
    return 499;
  case 2: // UNKNOWN
    return 500;
  case 3: // INVALID_ARGUMENT
    return 400;
  case 4: // DEADLINE_EXCEEDED
    return 504;
  case 5: // NOT_FOUND
    return 404;
  case 6: // ALREADY_EXISTS
    return 409;
  case 7: // PERMISSION_DENIED
    return 403;
  case 8: // RESOURCE_EXHAUSTED
    return 429;
  case 9: // FAILED_PRECONDITION
    return 400;
  case 10: // ABORTED
    return 409;
  case 11: // OUT_OF_RANGE
    return 400;
  case 12: // UNIMPLEMENTED
    return 501;
  case 13: // INTERNAL
    return 500;
  case 14: // UNAVAILABLE
    return 503;
  case 15: // DATA_LOSS
    return 500;
  case 16: // UNAUTHENTICATED
    return 401;
  default:
    return 500;
  }
}

void addHttpSpecificTags(const ::Wasm::Common::RequestInfo& request_info,
                         TagKeyValueList& tag_map) {
  const auto& operation = request_info.request_protocol == ::Wasm::Common::Protocol::GRPC
                              ? request_info.url_path
                              : request_info.request_operation;
  tag_map.emplace_back(Metric::requestOperationKey(), operation);

  const auto& response_code = request_info.request_protocol == ::Wasm::Common::Protocol::GRPC
                                  ? httpCodeFromGrpc(request_info.grpc_status)
                                  : request_info.response_code;
  tag_map.emplace_back(Metric::responseCodeKey(), std::to_string(response_code));
}

TagKeyValueList getMetricTagMap(const TagKeyValueList& input_map,
                                const TagKeyValueList& tag_overrides) {
  if (tag_overrides.empty()) {
    return input_map;
  }

  TagKeyValueList out;
  for (const auto& [tag_key, value_list] : input_map) {
    const auto& name = tag_key.name();
    auto it = std::find_if(tag_overrides.begin(), tag_overrides.end(),
                           [&name](const auto& override) { return override.first.name() == name; });
    if (it != tag_overrides.end()) {
      out.emplace_back(tag_key, it->second);
    } else {
      out.emplace_back(tag_key, value_list);
    }
  }

  auto it = std::find_if(tag_overrides.begin(), tag_overrides.end(), [](const auto& override) {
    return override.first.name() == "api_version";
  });
  if (it != tag_overrides.end()) {
    out.emplace_back(apiVersionKey(), it->second);
  }

  it = std::find_if(tag_overrides.begin(), tag_overrides.end(),
                    [](const auto& override) { return override.first.name() == "api_name"; });
  if (it != tag_overrides.end()) {
    out.emplace_back(apiNameKey(), it->second);
  }

  return out;
}

bool hasOverridesMatching(const override_map& overrides, const std::string& metric) {
  if (overrides.empty()) {
    return false;
  }
  auto it = std::find_if(
      overrides.begin(), overrides.end(),
      [&metric](const override_map_value_type& vt) { return absl::StrContains(vt.first, metric); });
  return it != overrides.end();
}

} // namespace

void record(bool is_outbound, const ::Wasm::Common::FlatNode& local_node_info,
            const ::Wasm::Common::FlatNode& peer_node_info,
            const ::Wasm::Common::RequestInfo& request_info, bool record_http_size_metrics,
            const override_map& overrides) {
  double latency_ms = request_info.duration /* in nanoseconds */ / 1000000.0;
  if (is_outbound) {
    TagKeyValueList tagMap = getOutboundTagMap(local_node_info, peer_node_info, request_info);
    addHttpSpecificTags(request_info, tagMap);

    if (hasOverridesMatching(overrides, "client")) {
      auto it = overrides.find(Common::kClientRequestCountView);
      if (it == overrides.end()) {
        opencensus::stats::Record({{clientRequestCountMeasure(), 1}}, tagMap);
      } else {
        opencensus::stats::Record({{clientRequestCountMeasure(), 1}},
                                  getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientRoundtripLatenciesView);
      if (it == overrides.end()) {
        opencensus::stats::Record({{clientRoundtripLatenciesMeasure(), latency_ms}}, tagMap);
      } else {
        opencensus::stats::Record({{clientRoundtripLatenciesMeasure(), latency_ms}},
                                  getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientRequestBytesView);
      if (it == overrides.end()) {
        opencensus::stats::Record({{clientRequestBytesMeasure(), request_info.request_size}},
                                  tagMap);
      } else {
        opencensus::stats::Record({{clientRequestBytesMeasure(), request_info.request_size}},
                                  getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientResponseBytesView);
      if (it == overrides.end()) {
        opencensus::stats::Record({{clientResponseBytesMeasure(), request_info.response_size}},
                                  tagMap);
      } else {
        opencensus::stats::Record({{clientResponseBytesMeasure(), request_info.response_size}},
                                  getMetricTagMap(tagMap, it->second));
      }
      return;
    }

    if (record_http_size_metrics) {
      opencensus::stats::Record({{clientRequestCountMeasure(), 1},
                                 {clientRoundtripLatenciesMeasure(), latency_ms},
                                 {clientRequestBytesMeasure(), request_info.request_size},
                                 {clientResponseBytesMeasure(), request_info.response_size}},
                                tagMap);
    } else {
      opencensus::stats::Record(
          {{clientRequestCountMeasure(), 1}, {clientRoundtripLatenciesMeasure(), latency_ms}},
          tagMap);
    }

    return;
  }

  TagKeyValueList tagMap = getInboundTagMap(local_node_info, peer_node_info, request_info);
  addHttpSpecificTags(request_info, tagMap);

  if (hasOverridesMatching(overrides, "server")) {
    auto it = overrides.find(Common::kServerRequestCountView);
    if (it == overrides.end()) {
      opencensus::stats::Record({{serverRequestCountMeasure(), 1}}, tagMap);
    } else {
      opencensus::stats::Record({{serverRequestCountMeasure(), 1}},
                                getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerResponseLatenciesView);
    if (it == overrides.end()) {
      opencensus::stats::Record({{serverResponseLatenciesMeasure(), latency_ms}}, tagMap);
    } else {
      opencensus::stats::Record({{serverResponseLatenciesMeasure(), latency_ms}},
                                getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerRequestBytesView);
    if (it == overrides.end()) {
      opencensus::stats::Record({{serverRequestBytesMeasure(), request_info.request_size}}, tagMap);
    } else {
      opencensus::stats::Record({{serverRequestBytesMeasure(), request_info.request_size}},
                                getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerResponseBytesView);
    if (it == overrides.end()) {
      opencensus::stats::Record({{serverResponseBytesMeasure(), request_info.response_size}},
                                tagMap);
    } else {
      opencensus::stats::Record({{serverResponseBytesMeasure(), request_info.response_size}},
                                getMetricTagMap(tagMap, it->second));
    }
    return;
  }

  if (record_http_size_metrics) {
    opencensus::stats::Record({{serverRequestCountMeasure(), 1},
                               {serverResponseLatenciesMeasure(), latency_ms},
                               {serverRequestBytesMeasure(), request_info.request_size},
                               {serverResponseBytesMeasure(), request_info.response_size}},
                              tagMap);
  } else {
    opencensus::stats::Record(
        {{serverRequestCountMeasure(), 1}, {serverResponseLatenciesMeasure(), latency_ms}}, tagMap);
  }
}

void recordTCP(bool is_outbound, const ::Wasm::Common::FlatNode& local_node_info,
               const ::Wasm::Common::FlatNode& peer_node_info,
               const ::Wasm::Common::RequestInfo& request_info, const override_map& overrides) {
  if (is_outbound) {
    TagKeyValueList tagMap = getOutboundTagMap(local_node_info, peer_node_info, request_info);

    if (hasOverridesMatching(overrides, "client")) {
      auto it = overrides.find(Common::kClientConnectionsOpenCountView);
      if (it == overrides.end()) {
        opencensus::stats::Record(
            {{clientConnectionsOpenCountMeasure(), request_info.tcp_connections_opened}}, tagMap);
      } else {
        opencensus::stats::Record(
            {{clientConnectionsOpenCountMeasure(), request_info.tcp_connections_opened}},
            getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientConnectionsCloseCountView);
      if (it == overrides.end()) {
        opencensus::stats::Record(
            {{clientConnectionsCloseCountMeasure(), request_info.tcp_connections_closed}}, tagMap);
      } else {
        opencensus::stats::Record(
            {{clientConnectionsCloseCountMeasure(), request_info.tcp_connections_closed}},
            getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientReceivedBytesCountView);
      if (it == overrides.end()) {
        opencensus::stats::Record(
            {{clientReceivedBytesCountMeasure(), request_info.tcp_received_bytes}}, tagMap);
      } else {
        opencensus::stats::Record(
            {{clientReceivedBytesCountMeasure(), request_info.tcp_received_bytes}},
            getMetricTagMap(tagMap, it->second));
      }

      it = overrides.find(Common::kClientSentBytesCountView);
      if (it == overrides.end()) {
        opencensus::stats::Record({{clientSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
                                  tagMap);
      } else {
        opencensus::stats::Record({{clientSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
                                  getMetricTagMap(tagMap, it->second));
      }
      return;
    }
    opencensus::stats::Record(
        {{clientConnectionsOpenCountMeasure(), request_info.tcp_connections_opened},
         {clientConnectionsCloseCountMeasure(), request_info.tcp_connections_closed},
         {clientReceivedBytesCountMeasure(), request_info.tcp_received_bytes},
         {clientSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
        tagMap);
    return;
  }

  TagKeyValueList tagMap = getInboundTagMap(local_node_info, peer_node_info, request_info);
  if (hasOverridesMatching(overrides, "server")) {
    auto it = overrides.find(Common::kServerConnectionsOpenCountView);
    if (it == overrides.end()) {
      opencensus::stats::Record(
          {{serverConnectionsOpenCountMeasure(), request_info.tcp_connections_opened}}, tagMap);
    } else {
      opencensus::stats::Record(
          {{serverConnectionsOpenCountMeasure(), request_info.tcp_connections_opened}},
          getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerConnectionsCloseCountView);
    if (it == overrides.end()) {
      opencensus::stats::Record(
          {{serverConnectionsCloseCountMeasure(), request_info.tcp_connections_closed}}, tagMap);
    } else {
      opencensus::stats::Record(
          {{serverConnectionsCloseCountMeasure(), request_info.tcp_connections_closed}},
          getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerReceivedBytesCountView);
    if (it == overrides.end()) {
      opencensus::stats::Record(
          {{serverReceivedBytesCountMeasure(), request_info.tcp_received_bytes}}, tagMap);
    } else {
      opencensus::stats::Record(
          {{serverReceivedBytesCountMeasure(), request_info.tcp_received_bytes}},
          getMetricTagMap(tagMap, it->second));
    }

    it = overrides.find(Common::kServerSentBytesCountView);
    if (it == overrides.end()) {
      opencensus::stats::Record({{serverSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
                                tagMap);
    } else {
      opencensus::stats::Record({{serverSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
                                getMetricTagMap(tagMap, it->second));
    }
    return;
  }
  opencensus::stats::Record(
      {{serverConnectionsOpenCountMeasure(), request_info.tcp_connections_opened},
       {serverConnectionsCloseCountMeasure(), request_info.tcp_connections_closed},
       {serverReceivedBytesCountMeasure(), request_info.tcp_received_bytes},
       {serverSentBytesCountMeasure(), request_info.tcp_sent_bytes}},
      tagMap);
  return;
}

} // namespace Metric
} // namespace Stackdriver
} // namespace Extensions