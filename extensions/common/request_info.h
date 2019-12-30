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
#include <unordered_map>

#include "absl/strings/string_view.h"
#include "extensions/common/node_info.pb.h"
#include "extensions/common/request_info.pb.h"

namespace Wasm {
namespace Common {

using StringView = absl::string_view;

const std::set<std::string> kGrpcContentTypes{
    "application/grpc", "application/grpc+proto", "application/grpc+json"};

// Header keys
constexpr StringView kAuthorityHeaderKey = ":authority";
constexpr StringView kMethodHeaderKey = ":method";
constexpr StringView kContentTypeHeaderKey = "content-type";

const std::string kProtocolHTTP = "http";
const std::string kProtocolGRPC = "grpc";

constexpr StringView kMutualTLS = "MUTUAL_TLS";
constexpr StringView kNone = "NONE";

enum class ServiceAuthenticationPolicy : int64_t {
  Unspecified = 0,
  None = 1,
  MutualTLS = 2,
};

// TrafficDirection is a mirror of envoy xDS traffic direction.
enum class TrafficDirection : int64_t {
  Unspecified = 0,
  Inbound = 1,
  Outbound = 2,
};

// RequestInfo lazily load request related information. It caches the request
// attribute the first fetching it from the host. Since this is stateful, it
// should only be used for telemetry.
class RequestInfo {
 public:
  virtual ~RequestInfo(){};

  virtual const google::protobuf::Timestamp& requestTimestamp() = 0;
  virtual const google::protobuf::Timestamp& responseTimestamp() = 0;
  virtual int64_t requestSize() = 0;
  virtual int64_t responseSize() = 0;
  virtual int64_t destinationPort() = 0;
  virtual const std::string& sourceAddress() = 0;
  virtual const std::string& destinationAddress() = 0;
  virtual const std::string& requestProtocol() = 0;
  virtual int64_t responseCode() = 0;
  virtual const std::string& responseFlag() = 0;
  virtual const std::string& destinationServiceName() = 0;
  virtual const std::string& destinationServiceHost() = 0;
  virtual const std::string& requestOperation() = 0;
  virtual ::Wasm::Common::ServiceAuthenticationPolicy
  serviceAuthenticationPolicy() = 0;
  virtual const std::string& sourcePrincipal() = 0;
  virtual const std::string& destinationPrincipal() = 0;
  virtual const std::string& rbacPermissivePolicyID() = 0;
  virtual const std::string& rbacPermissiveEngineResult() = 0;
  virtual const google::protobuf::Duration& duration() = 0;
  virtual const google::protobuf::Duration& responseDuration() = 0;
  virtual const std::string& requestedServerName() = 0;
  virtual bool isOutbound() = 0;

  virtual const std::string& referer() = 0;
  virtual const std::string& userAgent() = 0;
  virtual const std::string& urlPath() = 0;
  virtual const std::string& requestHost() = 0;
  virtual const std::string& requestScheme() = 0;
  virtual const std::string& requestID() = 0;
  virtual const std::string& b3TraceID() = 0;
  virtual const std::string& b3SpanID() = 0;
  virtual bool b3TraceSampled() = 0;
};

class RequestInfoImpl : public RequestInfo {
 public:
  RequestInfoImpl(const wasm::common::NodeInfo& dest_node,
                  bool use_traffic_data)
      : destination_namespace_(dest_node.namespace_()),
        use_traffic_data_(use_traffic_data) {}
  ~RequestInfoImpl() {}

  const google::protobuf::Timestamp& requestTimestamp() override;
  const google::protobuf::Timestamp& responseTimestamp() override;
  int64_t requestSize() override;
  int64_t responseSize() override;
  int64_t destinationPort() override;
  const std::string& sourceAddress() override;
  const std::string& destinationAddress() override;
  const std::string& requestProtocol() override;
  int64_t responseCode() override;
  const std::string& responseFlag() override;
  const std::string& destinationServiceName() override;
  const std::string& destinationServiceHost() override;
  const std::string& requestOperation() override;
  ::Wasm::Common::ServiceAuthenticationPolicy serviceAuthenticationPolicy()
      override;
  const std::string& sourcePrincipal() override;
  const std::string& destinationPrincipal() override;
  const std::string& rbacPermissivePolicyID() override;
  const std::string& rbacPermissiveEngineResult() override;
  const google::protobuf::Duration& duration() override;
  const google::protobuf::Duration& responseDuration() override;
  const std::string& requestedServerName() override;
  bool isOutbound() override;

  // Important headers
  const std::string& referer() override;
  const std::string& userAgent() override;
  const std::string& urlPath() override;
  const std::string& requestHost() override;
  const std::string& requestScheme() override;
  const std::string& requestID() override;
  const std::string& b3TraceID() override;
  const std::string& b3SpanID() override;
  bool b3TraceSampled() override;

 private:
  ::wasm::common::RequestInfo request_info_;
  std::string destination_namespace_;
  bool use_traffic_data_;
};

StringView AuthenticationPolicyString(ServiceAuthenticationPolicy policy);

}  // namespace Common
}  // namespace Wasm
