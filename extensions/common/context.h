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

#include <set>

#include "absl/strings/string_view.h"
#include "extensions/common/node_info_generated.h"
#include "flatbuffers/flatbuffers.h"

namespace Wasm {
namespace Common {

using StringView = absl::string_view;

// Node metadata
constexpr StringView WholeNodeKey = ".";

constexpr StringView kUpstreamMetadataIdKey = "upstream_peer_id";
constexpr StringView kUpstreamMetadataKey = "upstream_peer";

constexpr StringView kDownstreamMetadataIdKey = "downstream_peer_id";
constexpr StringView kDownstreamMetadataKey = "downstream_peer";

const std::string kMetadataNotFoundValue =
    "envoy.wasm.metadata_exchange.peer_unknown";

constexpr StringView kAccessLogPolicyKey = "istio.access_log_policy";

// Header keys
constexpr StringView kAuthorityHeaderKey = ":authority";
constexpr StringView kMethodHeaderKey = ":method";
constexpr StringView kContentTypeHeaderKey = "content-type";
constexpr StringView kEnvoyOriginalDstHostKey = "x-envoy-original-dst-host";
constexpr StringView kEnvoyOriginalPathKey = "x-envoy-original-path";

const std::string kProtocolHTTP = "http";
const std::string kProtocolGRPC = "grpc";
const std::string kProtocolTCP = "tcp";

constexpr absl::string_view kCanonicalServiceLabelName =
    "service.istio.io/canonical-name";
constexpr absl::string_view kCanonicalServiceRevisionLabelName =
    "service.istio.io/canonical-revision";
constexpr absl::string_view kLatest = "latest";

const std::set<std::string> kGrpcContentTypes{
    "application/grpc", "application/grpc+proto", "application/grpc+json"};

const std::set<std::string> kDefaultLabels{
    "app",
    "version",
    "service.istio.io/canonical-name",
    "service.istio.io/canonical-revision",
};

enum class ServiceAuthenticationPolicy : int64_t {
  Unspecified = 0,
  None = 1,
  MutualTLS = 2,
};

enum class TCPConnectionState : int64_t {
  Unspecified = 0,
  Open = 1,
  Connected = 2,
  Close = 3,
};

constexpr StringView kMutualTLS = "MUTUAL_TLS";
constexpr StringView kNone = "NONE";
constexpr StringView kOpen = "OPEN";
constexpr StringView kConnected = "CONNECTED";
constexpr StringView kClose = "CLOSE";

StringView AuthenticationPolicyString(ServiceAuthenticationPolicy policy);
StringView TCPConnectionStateString(TCPConnectionState state);

// RequestInfo represents the information collected from filter stream
// callbacks. This is used to fill metrics and logs.
struct RequestInfo {
  // Start timestamp in nanoseconds.
  int64_t start_time;

  // The total duration of the request in nanoseconds.
  int64_t duration;

  // Request total size in bytes, include header, body, and trailer.
  int64_t request_size = 0;

  // Response total size in bytes, include header, body, and trailer.
  int64_t response_size = 0;

  // Destination port that the request targets.
  uint32_t destination_port = 0;

  // Source port of the client.
  uint64_t source_port = 0;

  // Protocol used the request (HTTP/1.1, gRPC, etc).
  std::string request_protocol;

  // Response code of the request.
  uint32_t response_code = 0;

  // gRPC status code for the request.
  uint32_t grpc_status = 2;

  // Response flag giving additional information - NR, UAEX etc.
  // TODO populate
  std::string response_flag;

  // Host name of destination service.
  std::string destination_service_host;

  // Short name of destination service.
  std::string destination_service_name;

  // Operation of the request, i.e. HTTP method or gRPC API method.
  std::string request_operation;

  // The path portion of the URL without the query string.
  std::string request_url_path;

  // Service authentication policy (NONE, MUTUAL_TLS)
  ServiceAuthenticationPolicy service_auth_policy =
      ServiceAuthenticationPolicy::Unspecified;

  // Principal of source and destination workload extracted from TLS
  // certificate.
  std::string source_principal;
  std::string destination_principal;

  // Connection id of the TCP connection.
  uint64_t connection_id;

  // The following fields will only be populated by calling
  // populateExtendedHTTPRequestInfo.
  std::string source_address;
  std::string destination_address;

  // Additional fields for access log.
  std::string route_name;
  std::string upstream_host;
  std::string upstream_cluster;
  std::string request_serever_name;
  std::string x_envoy_original_path;
  std::string x_envoy_original_dst_host;

  // Important Headers.
  std::string referer;
  std::string user_agent;
  std::string request_id;
  std::string b3_trace_id;
  std::string b3_span_id;
  bool b3_trace_sampled = false;

  // HTTP URL related attributes.
  std::string url_path;
  std::string url_host;
  std::string url_scheme;

  // TCP variables.
  int64_t tcp_connections_opened = 0;
  int64_t tcp_connections_closed = 0;
  int64_t tcp_sent_bytes = 0;
  int64_t tcp_received_bytes = 0;
  int64_t tcp_total_sent_bytes = 0;
  int64_t tcp_total_received_bytes = 0;
  TCPConnectionState tcp_connection_state = TCPConnectionState::Unspecified;

  bool is_populated = false;
  bool log_sampled = false;
};

// RequestContext contains all the information available in the request.
// Some or all part may be populated depending on need.
struct RequestContext {
  const bool outbound;
  const Common::RequestInfo& request;
};

// TrafficDirection is a mirror of envoy xDS traffic direction.
enum class TrafficDirection : int64_t {
  Unspecified = 0,
  Inbound = 1,
  Outbound = 2,
};

// Retrieves the traffic direction from the configuration context.
TrafficDirection getTrafficDirection();

// Convenience routine to create an empty node flatbuffer.
void extractEmptyNodeFlatBuffer(std::string* out);

// Extra partial local node metadata into a flatbuffer.
// This populates a subset of nested labels and platform metadata to avoid
// parsing a protobuf from the host.
// See https://github.com/envoyproxy/envoy-wasm/issues/485.
bool extractPartialLocalNodeFlatBuffer(std::string* out);

// Returns flatbuffer schema for node info.
absl::string_view nodeInfoSchema();

// populateHTTPRequestInfo populates the RequestInfo struct. It needs access to
// the request context.
void populateHTTPRequestInfo(bool outbound, bool use_host_header,
                             RequestInfo* request_info,
                             const std::string& destination_namespace);

// populateExtendedHTTPRequestInfo populates the extra fields in RequestInfo
// struct, includes trace headers, request id headers, and url.
void populateExtendedHTTPRequestInfo(RequestInfo* request_info);

// populateExtendedRequestInfo populates the extra fields in RequestInfo
// source address, destination address.
void populateExtendedRequestInfo(RequestInfo* request_info);

// populateTCPRequestInfo populates the RequestInfo struct. It needs access to
// the request context.
void populateTCPRequestInfo(bool outbound, RequestInfo* request_info,
                            const std::string& destination_namespace);

}  // namespace Common
}  // namespace Wasm
