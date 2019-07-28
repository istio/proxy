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

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf::util;

Status extractNodeMetadata(const google::protobuf::Struct &metadata,
                           common::NodeInfo *node_info) {
  JsonOptions json_options;
  std::string metadata_json_struct;
  auto status =
      MessageToJsonString(metadata, &metadata_json_struct, json_options);
  if (status != Status::OK) {
    return status;
  }
  JsonParseOptions json_parse_options;
  json_parse_options.ignore_unknown_fields = true;
  return JsonStringToMessage(metadata_json_struct, node_info,
                             json_parse_options);
}

void initializeRequestInfo(RequestInfo *request_info) {
  // TODO: switch to stream_info.requestComplete() to avoid extra compute.
  request_info->end_timestamp = proxy_getCurrentTimeNanoseconds();

  // Fill in request info.
  request_info->response_code = getResponseCode(StreamType::Response);
  request_info->request_protocol = getProtocol(StreamType::Request)->toString();
  request_info->destination_service_host =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kAuthorityHeaderKey)
          ->toString();
  request_info->request_operation =
      getHeaderMapValue(HeaderMapType::RequestHeaders, kMethodHeaderKey)
          ->toString();
  request_info->destination_port = getDestinationPort(StreamType::Request);

  // Fill in peer node metadata in request info.
  auto downstream_metadata =
      getMetadataStruct(MetadataType::Request, kDownstreamMetadataKey);
  auto status = extractNodeMetadata(downstream_metadata,
                                    &(request_info->downstream_node_info));
  if (status != Status::OK) {
    logWarn("cannot parse downstream peer node metadata " +
            downstream_metadata.DebugString() + ": " + status.ToString());
  }
  auto upstream_metadata =
      getMetadataStruct(MetadataType::Request, kUpstreamMetadataKey);
  status = extractNodeMetadata(upstream_metadata,
                               &(request_info->upstream_node_info));
  if (status != Status::OK) {
    logWarn("cannot parse upstream peer node metadata " +
            upstream_metadata.DebugString() + ": " + status.ToString());
  }
}

}  // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif