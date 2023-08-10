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

#include "extensions/common/node_info_generated.h"
#include "flatbuffers/flatbuffers.h"

namespace Wasm {
namespace Common {

// Node metadata
constexpr std::string_view WholeNodeKey = ".";

constexpr std::string_view kUpstreamMetadataIdKey = "upstream_peer_id";
constexpr std::string_view kUpstreamMetadataKey = "upstream_peer";

constexpr std::string_view kDownstreamMetadataIdKey = "downstream_peer_id";
constexpr std::string_view kDownstreamMetadataKey = "downstream_peer";

// Sentinel key in the filter state, indicating that the peer metadata is
// decidedly absent. This is different from a missing peer metadata ID key
// which could indicate that the metadata is not received yet.
const std::string kMetadataNotFoundValue = "envoy.wasm.metadata_exchange.peer_unknown";

constexpr std::string_view kAccessLogPolicyKey = "istio.access_log_policy";
constexpr std::string_view kRequestOperationKey = "istio_operationId";

// Header keys
constexpr std::string_view kAuthorityHeaderKey = ":authority";
constexpr std::string_view kMethodHeaderKey = ":method";
constexpr std::string_view kContentTypeHeaderKey = "content-type";
constexpr std::string_view kEnvoyOriginalDstHostKey = "x-envoy-original-dst-host";
constexpr std::string_view kEnvoyOriginalPathKey = "x-envoy-original-path";

constexpr std::string_view kProtocolHTTP = "http";
constexpr std::string_view kProtocolGRPC = "grpc";
constexpr std::string_view kProtocolTCP = "tcp";

constexpr std::string_view kCanonicalServiceLabelName = "service.istio.io/canonical-name";
constexpr std::string_view kCanonicalServiceRevisionLabelName =
    "service.istio.io/canonical-revision";
constexpr std::string_view kLatest = "latest";

const std::set<std::string> kGrpcContentTypes{"application/grpc", "application/grpc+proto",
                                              "application/grpc+json"};

enum class ServiceAuthenticationPolicy : uint8_t {
  Unspecified = 0,
  None = 1,
  MutualTLS = 2,
};

enum class TCPConnectionState : uint8_t {
  Unspecified = 0,
  Open = 1,
  Connected = 2,
  Close = 3,
};

enum class Protocol : uint32_t {
  Unspecified = 0x0,
  TCP = 0x1,
  HTTP = 0x2,
  GRPC = 0x4,
};

constexpr std::string_view kMutualTLS = "MUTUAL_TLS";
constexpr std::string_view kNone = "NONE";
constexpr std::string_view kOpen = "OPEN";
constexpr std::string_view kConnected = "CONNECTED";
constexpr std::string_view kClose = "CLOSE";

std::string_view AuthenticationPolicyString(ServiceAuthenticationPolicy policy);
std::string_view TCPConnectionStateString(TCPConnectionState state);
std::string_view ProtocolString(Protocol protocol);

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
  Protocol request_protocol = Protocol::Unspecified;

  // Response code of the request.
  uint32_t response_code = 0;

  // gRPC status code for the request.
  uint32_t grpc_status = 2;

  // Response flag giving additional information - NR, UAEX etc.
  std::string response_flag;

  // Host name of destination service.
  std::string destination_service_host;

  // Short name of destination service.
  std::string destination_service_name;

  // Operation of the request, i.e. HTTP method or gRPC API method.
  std::string request_operation;

  std::string upstream_transport_failure_reason;

  // Service authentication policy (NONE, MUTUAL_TLS)
  ServiceAuthenticationPolicy service_auth_policy = ServiceAuthenticationPolicy::Unspecified;

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
  std::string response_details;

  // Additional fields for access log.
  std::string route_name;
  std::string upstream_host;
  std::string upstream_cluster;
  std::string requested_server_name;
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
  // The path portion of the URL including the query string.
  std::string path;
  // The path portion of the URL without the query string.
  std::string url_path;
  std::string url_host;
  std::string url_scheme;

  // TCP variables.
  uint8_t tcp_connections_opened = 0;
  uint8_t tcp_connections_closed = 0;
  uint64_t tcp_sent_bytes = 0;
  uint64_t tcp_received_bytes = 0;
  uint64_t tcp_total_sent_bytes = 0;
  uint64_t tcp_total_received_bytes = 0;
  TCPConnectionState tcp_connection_state = TCPConnectionState::Unspecified;

  bool is_populated = false;
  bool log_sampled = false;

  // gRPC variables.
  uint64_t request_message_count = 0;
  uint64_t response_message_count = 0;
  uint64_t last_request_message_count = 0;
  uint64_t last_response_message_count = 0;
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
flatbuffers::DetachedBuffer extractEmptyNodeFlatBuffer();

// Extract local node metadata into a flatbuffer. Detached buffer owns the
// underlying heap-allocated memory. Note that std::string is inappropriate here
// because its memory is inlined for short strings and causes a misaligned
// address access.
flatbuffers::DetachedBuffer extractLocalNodeFlatBuffer();

// Extract upstream peer metadata from upstream host metadata.
// Returns true if the metadata is found in the upstream host metadata.
bool extractPeerMetadataFromUpstreamHostMetadata(flatbuffers::FlatBufferBuilder& fbb);

// Extract upstream peer metadata from upstream cluster metadata.
// Returns true if the metadata is found in the upstream cluster metadata.
bool extractPeerMetadataFromUpstreamClusterMetadata(flatbuffers::FlatBufferBuilder& fbb);

// Returns flatbuffer schema for node info.
std::string_view nodeInfoSchema();

class PeerNodeInfo {
public:
  explicit PeerNodeInfo(const std::string_view peer_metadata_id_key,
                        const std::string_view peer_metadata_key);
  PeerNodeInfo() = delete;
  const ::Wasm::Common::FlatNode& get() const;
  const std::string& id() const { return peer_id_; }

  // Found indicates whether both ID and metadata is available.
  bool found() const { return found_; }

  // Maybe waiting indicates that the metadata is not found but may arrive
  // later.
  bool maybeWaiting() const {
    return !found_ && peer_id_ != ::Wasm::Common::kMetadataNotFoundValue;
  }

private:
  bool found_;
  std::string peer_id_;
  std::string peer_node_;
  flatbuffers::DetachedBuffer fallback_peer_node_;
};

// Populate shared information between all protocols.
// Requires that the connections are established both downstrean and upstream.
// Caches computation using is_populated field.
void populateRequestInfo(bool outbound, bool use_host_header_fallback, RequestInfo* request_info);

// populateHTTPRequestInfo populates the RequestInfo struct. It needs access to
// the request context.
void populateHTTPRequestInfo(bool outbound, bool use_host_header, RequestInfo* request_info);

// populateExtendedHTTPRequestInfo populates the extra fields in RequestInfo
// struct, includes trace headers, request id headers, and url.
void populateExtendedHTTPRequestInfo(RequestInfo* request_info);

// populateExtendedRequestInfo populates the extra fields in RequestInfo
// source address, destination address.
void populateExtendedRequestInfo(RequestInfo* request_info);

// populateTCPRequestInfo populates the RequestInfo struct. It needs access to
// the request context.
void populateTCPRequestInfo(bool outbound, RequestInfo* request_info);

// Detect HTTP and gRPC request protocols.
void populateRequestProtocol(RequestInfo* request_info);

// populateGRPCInfo fills gRPC-related information, such as message counts.
// Returns true if all information is filled.
bool populateGRPCInfo(RequestInfo* request_info);

// Read value of 'access_log_hint' key from envoy dynamic metadata which
// determines whether to audit a request or not.
bool getAuditPolicy();

// Returns a string view stored in a flatbuffers string.
static inline std::string_view GetFromFbStringView(const flatbuffers::String* str) {
  return str ? std::string_view(str->c_str(), str->size()) : std::string_view();
}

// Sanitizes a possible UTF-8 byte buffer to a UTF-8 string.
// Invalid byte sequences are replaced by spaces.
// Returns true if the string was modified.
bool sanitizeBytes(std::string* buf);

std::string getServiceNameFallback();

} // namespace Common
} // namespace Wasm
