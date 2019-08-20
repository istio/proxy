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
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null_plugin.h"

using Envoy::Extensions::Common::Wasm::HeaderMapType;
using Envoy::Extensions::Common::Wasm::StreamType;
using Envoy::Extensions::Common::Wasm::MetadataType;
using Envoy::Extensions::Common::Wasm::MetadataResult;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getHeaderMapValue;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getMetadataStruct;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getRequestDestinationPort;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getRequestTlsVersion;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getResponseResponseCode;
using Envoy::Extensions::Common::Wasm::Null::Plugin::getResponseTlsVersion;
using Envoy::Extensions::Common::Wasm::Null::Plugin::
    proxy_getCurrentTimeNanoseconds;

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Wasm {
namespace Common {

using google::protobuf::util::JsonStringToMessage;
using google::protobuf::util::MessageToJsonString;

google::protobuf::util::Status extractNodeMetadata(
    const google::protobuf::Struct &metadata,
    wasm::common::NodeInfo *node_info) {
  google::protobuf::util::JsonOptions json_options;
  std::string metadata_json_struct;
  auto status =
      MessageToJsonString(metadata, &metadata_json_struct, json_options);
  if (status != google::protobuf::util::Status::OK) {
    return status;
  }
  google::protobuf::util::JsonParseOptions json_parse_options;
  json_parse_options.ignore_unknown_fields = true;
  return JsonStringToMessage(metadata_json_struct, node_info,
                             json_parse_options);
}

google::protobuf::util::Status extractLocalNodeMetadata(
    wasm::common::NodeInfo *node_info){
  google::protobuf::Struct node;
  if (getMetadataStruct(MetadataType::Node, "metadata",
                        &node) != MetadataResult::Ok){
    return google::protobuf::util::Status(
        google::protobuf::util::error::Code::NOT_FOUND, "metadata not found");
  }
  return extractNodeMetadata(node, node_info);
}

void populateHTTPRequestInfo(bool outbound, RequestInfo *request_info) {
  // TODO: switch to stream_info.requestComplete() to avoid extra compute.
  request_info->end_timestamp = proxy_getCurrentTimeNanoseconds();

  // Fill in request info.
  getResponseResponseCode(&request_info->response_code);

  if (kGrpcContentTypes.contains(
          getHeaderMapValue(HeaderMapType::RequestHeaders,
                            kContentTypeHeaderKey)
              ->toString())) {
    request_info->request_protocol = kProtocolGRPC;
  } else {
    // TODO Add http/1.1, http/1.0, http/2 in a separate attribute.
    // http|grpc classification is compatible with Mixerclient
    request_info->request_protocol = kProtocolHTTP;
  }

  request_info->destination_service_host =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kAuthorityHeaderKey)
          ->toString();
  request_info->request_operation =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kMethodHeaderKey)
          ->toString();
  getRequestDestinationPort(&request_info->destination_port);

  std::string tls_version;

  if (outbound) {
    getResponseTlsVersion(&tls_version);
  } else {
    getRequestTlsVersion(&tls_version);
  }

  // TODO (mjog) fix when peerCertificatePresented or equivalent is available.
  // At present tls == mTLS
  request_info->mTLS = !tls_version.empty();
}

}  // namespace Common
}  // namespace Wasm
