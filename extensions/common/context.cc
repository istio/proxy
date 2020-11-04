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

#include "extensions/common/context.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/node_info_bfbs_generated.h"
#include "extensions/common/util.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "absl/strings/str_split.h"
#include "extensions/common/wasm/null/null_plugin.h"

using Envoy::Extensions::Common::Wasm::HeaderMapType;
using Envoy::Extensions::Common::Wasm::WasmResult;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getCurrentTimeNanoseconds;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getHeaderMapValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getMessageValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getValue;

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Wasm {
namespace Common {

const char kBlackHoleCluster[] = "BlackHoleCluster";
const char kPassThroughCluster[] = "PassthroughCluster";
const char kBlackHoleRouteName[] = "block_all";
const char kPassThroughRouteName[] = "allow_any";
const char kInboundPassthroughClusterIpv4[] = "InboundPassthroughClusterIpv4";
const char kInboundPassthroughClusterIpv6[] = "InboundPassthroughClusterIpv6";

namespace {

// Extract service name from service host.
void extractServiceName(const std::string& host,
                        const std::string& destination_namespace,
                        std::string* service_name) {
  auto name_pos = host.find_first_of(".:");
  if (name_pos == std::string::npos) {
    // host is already a short service name. return it directly.
    *service_name = host;
    return;
  }
  if (host[name_pos] == ':') {
    // host is `short_service:port`, return short_service name.
    *service_name = host.substr(0, name_pos);
    return;
  }

  auto namespace_pos = host.find_first_of(".:", name_pos + 1);
  std::string service_namespace = "";
  if (namespace_pos == std::string::npos) {
    service_namespace = host.substr(name_pos + 1);
  } else {
    int namespace_size = namespace_pos - name_pos - 1;
    service_namespace = host.substr(name_pos + 1, namespace_size);
  }
  // check if namespace in host is same as destination namespace.
  // If it is the same, return the first part of host as service name.
  // Otherwise fallback to request host.
  if (service_namespace == destination_namespace) {
    *service_name = host.substr(0, name_pos);
  } else {
    *service_name = host;
  }
}

// Get destination service host and name based on destination cluster name and
// host header.
// * If cluster name is one of passthrough and blackhole clusters, use cluster
//   name as destination service name and host header as destination host.
// * If cluster name follows Istio convention (four parts separated by pipe),
//   use the last part as destination host; Otherwise, use host header as
//   destination host. To get destination service name from host: if destination
//   host is already a short name, use that as destination service; otherwise if
//   the second part of destination host is destination namespace, use first
//   part as destination service name. Otherwise, fallback to use destination
//   host for destination service name.
void getDestinationService(const std::string& dest_namespace,
                           bool use_host_header, std::string* dest_svc_host,
                           std::string* dest_svc_name) {
  std::string cluster_name;
  getValue({"cluster_name"}, &cluster_name);
  *dest_svc_host = use_host_header
                       ? getHeaderMapValue(HeaderMapType::RequestHeaders,
                                           kAuthorityHeaderKey)
                             ->toString()
                       : "unknown";

  // override the cluster name if this is being sent to the
  // blackhole or passthrough cluster
  std::string route_name;
  getValue({"route_name"}, &route_name);
  if (route_name == kBlackHoleRouteName) {
    cluster_name = kBlackHoleCluster;
  } else if (route_name == kPassThroughRouteName) {
    cluster_name = kPassThroughCluster;
  }

  if (cluster_name == kBlackHoleCluster ||
      cluster_name == kPassThroughCluster ||
      cluster_name == kInboundPassthroughClusterIpv4 ||
      cluster_name == kInboundPassthroughClusterIpv6) {
    *dest_svc_name = cluster_name;
    return;
  }

  std::vector<absl::string_view> parts = absl::StrSplit(cluster_name, '|');
  if (parts.size() == 4) {
    *dest_svc_host = std::string(parts[3].data(), parts[3].size());
  }

  extractServiceName(*dest_svc_host, dest_namespace, dest_svc_name);
}

void populateRequestInfo(bool outbound, bool use_host_header_fallback,
                         RequestInfo* request_info,
                         const std::string& destination_namespace) {
  request_info->is_populated = true;
  // Fill in request info.
  // Get destination service name and host based on cluster name and host
  // header.
  getDestinationService(destination_namespace, use_host_header_fallback,
                        &request_info->destination_service_host,
                        &request_info->destination_service_name);

  getValue({"request", "url_path"}, &request_info->request_url_path);

  if (outbound) {
    uint64_t destination_port = 0;
    getValue({"upstream", "port"}, &destination_port);
    request_info->destination_port = destination_port;
    getValue({"upstream", "uri_san_peer_certificate"},
             &request_info->destination_principal);
    getValue({"upstream", "uri_san_local_certificate"},
             &request_info->source_principal);
  } else {
    bool mtls = false;
    if (getValue({"connection", "mtls"}, &mtls)) {
      request_info->service_auth_policy =
          mtls ? ::Wasm::Common::ServiceAuthenticationPolicy::MutualTLS
               : ::Wasm::Common::ServiceAuthenticationPolicy::None;
    }
    getValue({"connection", "uri_san_local_certificate"},
             &request_info->destination_principal);
    getValue({"connection", "uri_san_peer_certificate"},
             &request_info->source_principal);
  }

  uint64_t response_flags = 0;
  getValue({"response", "flags"}, &response_flags);
  request_info->response_flag = parseResponseFlag(response_flags);
}

}  // namespace

StringView AuthenticationPolicyString(ServiceAuthenticationPolicy policy) {
  switch (policy) {
    case ServiceAuthenticationPolicy::None:
      return kNone;
    case ServiceAuthenticationPolicy::MutualTLS:
      return kMutualTLS;
    default:
      break;
  }
  return {};
  ;
}

// Retrieves the traffic direction from the configuration context.
TrafficDirection getTrafficDirection() {
  int64_t direction;
  if (getValue({"listener_direction"}, &direction)) {
    return static_cast<TrafficDirection>(direction);
  }
  return TrafficDirection::Unspecified;
}

void extractEmptyNodeFlatBuffer(std::string* out) {
  flatbuffers::FlatBufferBuilder fbb;
  FlatNodeBuilder node(fbb);
  auto data = node.Finish();
  fbb.Finish(data);
  out->assign(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
              fbb.GetSize());
}

bool extractPartialLocalNodeFlatBuffer(std::string* out) {
  flatbuffers::FlatBufferBuilder fbb;
  flatbuffers::Offset<flatbuffers::String> name, namespace_, owner,
      workload_name, istio_version, mesh_id, cluster_id;
  std::vector<flatbuffers::Offset<KeyVal>> labels;
  std::string value;
  if (getValue({"node", "metadata", "NAME"}, &value)) {
    name = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "NAMESPACE"}, &value)) {
    namespace_ = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "OWNER"}, &value)) {
    owner = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "WORKLOAD_NAME"}, &value)) {
    workload_name = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "ISTIO_VERSION"}, &value)) {
    istio_version = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "MESH_ID"}, &value)) {
    mesh_id = fbb.CreateString(value);
  }
  if (getValue({"node", "metadata", "CLUSTER_ID"}, &value)) {
    cluster_id = fbb.CreateString(value);
  }
  for (const auto& label : kDefaultLabels) {
    if (getValue({"node", "metadata", "LABELS", label}, &value)) {
      labels.push_back(
          CreateKeyVal(fbb, fbb.CreateString(label), fbb.CreateString(value)));
    }
  }

  auto labels_offset = fbb.CreateVectorOfSortedTables(&labels);
  FlatNodeBuilder node(fbb);
  node.add_name(name);
  node.add_namespace_(namespace_);
  node.add_owner(owner);
  node.add_workload_name(workload_name);
  node.add_istio_version(istio_version);
  node.add_mesh_id(mesh_id);
  node.add_cluster_id(cluster_id);
  node.add_labels(labels_offset);
  auto data = node.Finish();
  fbb.Finish(data);
  out->assign(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
              fbb.GetSize());
  return true;
}

// Host header is used if use_host_header_fallback==true.
// Normally it is ok to use host header within the mesh, but not at ingress.
void populateHTTPRequestInfo(bool outbound, bool use_host_header_fallback,
                             RequestInfo* request_info,
                             const std::string& destination_namespace) {
  populateRequestInfo(outbound, use_host_header_fallback, request_info,
                      destination_namespace);

  int64_t response_code = 0;
  if (getValue({"response", "code"}, &response_code)) {
    request_info->response_code = response_code;
  }

  int64_t grpc_status_code = 2;
  getValue({"response", "grpc_status"}, &grpc_status_code);
  request_info->grpc_status = grpc_status_code;

  if (kGrpcContentTypes.count(getHeaderMapValue(HeaderMapType::RequestHeaders,
                                                kContentTypeHeaderKey)
                                  ->toString()) != 0) {
    request_info->request_protocol = kProtocolGRPC;
  } else {
    // TODO Add http/1.1, http/1.0, http/2 in a separate attribute.
    // http|grpc classification is compatible with Mixerclient
    request_info->request_protocol = kProtocolHTTP;
  }

  request_info->request_operation =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kMethodHeaderKey)
          ->toString();

  if (!outbound) {
    uint64_t destination_port = 0;
    getValue({"destination", "port"}, &destination_port);
    request_info->destination_port = destination_port;
  }

  getValue({"request", "time"}, &request_info->start_time);
  getValue({"request", "duration"}, &request_info->duration);
  getValue({"request", "total_size"}, &request_info->request_size);
  getValue({"response", "total_size"}, &request_info->response_size);
}

absl::string_view nodeInfoSchema() {
  return absl::string_view(
      reinterpret_cast<const char*>(FlatNodeBinarySchema::data()),
      FlatNodeBinarySchema::size());
}

void populateExtendedHTTPRequestInfo(RequestInfo* request_info) {
  getValue({"source", "address"}, &request_info->source_address);
  getValue({"destination", "address"}, &request_info->destination_address);

  getValue({"request", "referer"}, &request_info->referer);
  getValue({"request", "useragent"}, &request_info->user_agent);
  getValue({"request", "id"}, &request_info->request_id);
  std::string trace_sampled;
  if (getValue({"request", "headers", "x-b3-sampled"}, &trace_sampled) &&
      trace_sampled == "1") {
    getValue({"request", "headers", "x-b3-traceid"},
             &request_info->b3_trace_id);
    getValue({"request", "headers", "x-b3-spanid"}, &request_info->b3_span_id);
    request_info->b3_trace_sampled = true;
  }

  getValue({"request", "url_path"}, &request_info->url_path);
  getValue({"request", "host"}, &request_info->url_host);
  getValue({"request", "scheme"}, &request_info->url_scheme);
}

void populateTCPRequestInfo(bool outbound, RequestInfo* request_info,
                            const std::string& destination_namespace) {
  // host_header_fallback is for HTTP/gRPC only.
  populateRequestInfo(outbound, false, request_info, destination_namespace);

  request_info->request_protocol = kProtocolTCP;
}

}  // namespace Common
}  // namespace Wasm
