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

#include "src/istio/context/http_builder.h"

#include "common/grpc/common.h"
#include "common/http/utility.h"
#include "common/stream_info/utility.h"

using ::Envoy::Http::HeaderMap;
using ::google::protobuf::StringValue;

namespace istio {
namespace context {

namespace {

// Set of headers excluded from request.headers attribute.
const std::set<std::string> kRequestHeaderExclusives = {"x-istio-attributes"};

// Set of headers excluded from response.headers attribute.
const std::set<std::string> kResponseHeaderExclusives = {};

void ExtractHeaders(::google::protobuf::Map<std::string, std::string>* out,
                    const std::set<std::string>& exclusives,
                    const HeaderMap& headers) {
  struct Context {
    Context(const std::set<std::string>& exclusives,
            ::google::protobuf::Map<std::string, std::string>* out)
        : exclusives(exclusives), out(out) {}
    const std::set<std::string>& exclusives;
    ::google::protobuf::Map<std::string, std::string>* out;
  };
  Context ctx(exclusives, out);
  headers.iterate(
      [](const ::Envoy::Http::HeaderEntry& header,
         void* context) -> ::Envoy::Http::HeaderMap::Iterate {
        Context* ctx = static_cast<Context*>(context);
        if (ctx->exclusives.count(header.key().c_str()) == 0) {
          (*ctx->out)[header.key().c_str()] = header.value().c_str();
        }
        return ::Envoy::Http::HeaderMap::Iterate::Continue;
      },
      &ctx);
}

void ExtractTimestamp(
    ::google::protobuf::Timestamp* time_stamp,
    const std::chrono::time_point<std::chrono::system_clock>& value) {
  long long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        value.time_since_epoch())
                        .count();
  time_stamp->set_seconds(nanos / 1000000000);
  time_stamp->set_nanos(nanos % 1000000000);
}

void ExtractDuration(::google::protobuf::Duration* duration,
                     const std::chrono::nanoseconds& value) {
  duration->set_seconds(value.count() / 1000000000);
  duration->set_nanos(value.count() % 1000000000);
}

bool ExtractGrpcStatus(const HeaderMap& headers, Response& response) {
  if (headers.GrpcStatus()) {
    *response.mutable_grpc_status()->mutable_value() =
        std::string(headers.GrpcStatus()->value().c_str(),
                    headers.GrpcStatus()->value().size());
    if (headers.GrpcMessage()) {
      *response.mutable_grpc_message()->mutable_value() =
          std::string(headers.GrpcMessage()->value().c_str(),
                      headers.GrpcMessage()->value().size());
    }
    return true;
  }
  return false;
}

}  // namespace

void ExtractRequest(Request& request, const HeaderMap& headers) {
  if (headers.Path()) {
    const ::Envoy::Http::HeaderString& path = headers.Path()->value();
    *request.mutable_path()->mutable_value() =
        std::string(path.c_str(), path.size());
    const char* query_start =
        ::Envoy::Http::Utility::findQueryStringStart(path);
    if (query_start != nullptr) {
      *request.mutable_url_path()->mutable_value() =
          std::string(path.c_str(), query_start - path.c_str());
    } else {
      *request.mutable_url_path()->mutable_value() =
          std::string(path.c_str(), path.size());
    }
    for (const auto& kv :
         ::Envoy::Http::Utility::parseQueryString(path.c_str())) {
      (*request.mutable_query_params())[kv.first] = kv.second;
    }
  }
  if (headers.Host()) {
    *request.mutable_host()->mutable_value() = std::string(
        headers.Host()->value().c_str(), headers.Host()->value().size());
  }
  if (headers.Scheme()) {
    *request.mutable_scheme()->mutable_value() = std::string(
        headers.Scheme()->value().c_str(), headers.Scheme()->value().size());
  }
  if (headers.UserAgent()) {
    *request.mutable_useragent()->mutable_value() =
        std::string(headers.UserAgent()->value().c_str(),
                    headers.UserAgent()->value().size());
  }
  if (headers.Method()) {
    *request.mutable_method()->mutable_value() = std::string(
        headers.Method()->value().c_str(), headers.Method()->value().size());
  }
  if (headers.Referer()) {
    *request.mutable_referer()->mutable_value() = std::string(
        headers.Referer()->value().c_str(), headers.Referer()->value().size());
  }

  ExtractHeaders(request.mutable_headers(), kRequestHeaderExclusives, headers);
  ExtractTimestamp(request.mutable_time(), std::chrono::system_clock::now());
}

void ExtractContext(Context& context, const HeaderMap& headers) {
  if (::Envoy::Grpc::Common::hasGrpcContentType(headers)) {
    *context.mutable_protocol()->mutable_value() = "grpc";
  } else {
    *context.mutable_protocol()->mutable_value() = "http";
  }
}

// TODO: check auth attributes

void ExtractConnection(Connection& connection,
                       const ::Envoy::Network::Connection& downstream) {
  if (!downstream.requestedServerName().empty()) {
    *connection.mutable_requested_server_name()->mutable_value() =
        std::string(downstream.requestedServerName());
  }
  if (downstream.ssl() != nullptr &&
      downstream.ssl()->peerCertificatePresented()) {
    connection.mutable_mtls()->set_value(true);
  }
}

void ExtractOrigin(Origin& origin,
                   const ::Envoy::Network::Connection& downstream) {
  auto ip = downstream.remoteAddress()->ip();
  if (ip) {
    if (ip->ipv4()) {
      uint32_t ipv4 = ip->ipv4()->address();
      *origin.mutable_ip()->mutable_value() =
          std::string(reinterpret_cast<const char*>(&ipv4), sizeof(ipv4));
    } else if (ip->ipv6()) {
      absl::uint128 ipv6 = ip->ipv6()->address();
      *origin.mutable_ip()->mutable_value() =
          std::string(reinterpret_cast<const char*>(&ipv6), 16);
    }
  }
}

void ExtractReportData(Request& request, Response& response, Context& context,
                       const HeaderMap& response_headers,
                       const HeaderMap& response_trailers,
                       const ::Envoy::StreamInfo::StreamInfo& info) {
  // TODO: RBAC permissive info
  // TODO: dynamic filter state
  // TODO: total_size for request and response
  // TODO: GetDestinationUID, IP, Port
  // TODO: check_status
  ExtractTimestamp(response.mutable_time(), std::chrono::system_clock::now());
  ExtractHeaders(response.mutable_headers(), kResponseHeaderExclusives,
                 response_headers);
  ExtractHeaders(response.mutable_headers(), kResponseHeaderExclusives,
                 response_trailers);

  *context.mutable_proxy_error_code()->mutable_value() =
      ::Envoy::StreamInfo::ResponseFlagUtils::toShortString(info);
  request.mutable_size()->set_value(info.bytesReceived());
  response.mutable_size()->set_value(info.bytesSent());
  response.mutable_code()->set_value(info.responseCode().value_or(500));

  ExtractDuration(response.mutable_duration(),
                  info.requestComplete().value_or(std::chrono::nanoseconds{0}));

  if (!ExtractGrpcStatus(response_trailers, response)) {
    ExtractGrpcStatus(response_headers, response);
  }
}

}  // namespace context
}  // namespace istio
