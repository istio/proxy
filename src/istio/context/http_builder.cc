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

#include "common/http/utility.h"

using ::Envoy::Http::HeaderMap;
using ::google::protobuf::StringValue;

namespace istio {
namespace context {

namespace {
// Referer header
const ::Envoy::Http::LowerCaseString kRefererHeaderKey("referer");

// Set of headers excluded from request.headers attribute.
const std::set<std::string> kRequestHeaderExclusives = {"x-istio-attributes"};

// The gRPC content types.
const std::set<std::string> kGrpcContentTypes{
    "application/grpc", "application/grpc+proto", "application/grpc+json"};

void extractTimestamp(
    ::google::protobuf::Timestamp* time_stamp,
    const std::chrono::time_point<std::chrono::system_clock>& value) {
  long long nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        value.time_since_epoch())
                        .count();
  time_stamp->set_seconds(nanos / 1000000000);
  time_stamp->set_nanos(nanos % 1000000000);
}

}  // namespace

void ExtractHeaders(Request& request, const HeaderMap& headers) {
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

  const ::Envoy::Http::HeaderEntry* referer = headers.get(kRefererHeaderKey);
  if (referer) {
    *request.mutable_referer()->mutable_value() =
        std::string(referer->value().c_str(), referer->value().size());
  }

  struct Context {
    Context(const std::set<std::string>& exclusives, Request& request)
        : exclusives(exclusives), request(request) {}
    const std::set<std::string>& exclusives;
    Request& request;
  };
  Context ctx(kRequestHeaderExclusives, request);
  headers.iterate(
      [](const ::Envoy::Http::HeaderEntry& header,
         void* context) -> ::Envoy::Http::HeaderMap::Iterate {
        Context* ctx = static_cast<Context*>(context);
        if (ctx->exclusives.count(header.key().c_str()) == 0) {
          (*ctx->request.mutable_headers())[header.key().c_str()] =
              header.value().c_str();
        }
        return ::Envoy::Http::HeaderMap::Iterate::Continue;
      },
      &ctx);

  // Populate request.time
  extractTimestamp(request.mutable_time(), std::chrono::system_clock::now());
}

void ExtractContext(Context& context, const HeaderMap& headers) {
  if (headers.ContentType() &&
      kGrpcContentTypes.count(headers.ContentType()->value().c_str()) != 0) {
    *context.mutable_protocol()->mutable_value() = "grpc";
  } else {
    *context.mutable_protocol()->mutable_value() = "http";
  }
}

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

}  // namespace context
}  // namespace istio
