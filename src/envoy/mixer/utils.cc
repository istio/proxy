/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "common/http/headers.h"
#include "src/envoy/mixer/utils.h"
#include "src/envoy/mixer/string_map.pb.h"

using ::google::protobuf::util::Status;
using StatusCode = ::google::protobuf::util::error::Code;

namespace Envoy {
namespace Http {
namespace Utils {

const LowerCaseString kIstioAttributeHeader("x-istio-attributes");

std::string SerializeTwoStringMaps(const StringMap &map1,
                                   const StringMap &map2) {
  ::istio::proxy::mixer::StringMap pb;
  ::google::protobuf::Map<std::string, std::string> *map_pb = pb.mutable_map();
  for (const auto &it : map1) {
    (*map_pb)[it.first] = it.second;
  }
  for (const auto &it : map2) {
    (*map_pb)[it.first] = it.second;
  }
  std::string str;
  pb.SerializeToString(&str);
  return str;
}

// Convert Status::code to HTTP code
int HttpCode(int code) {
  // Map Canonical codes to HTTP status codes. This is based on the mapping
  // defined by the protobuf http error space.
  switch (code) {
    case StatusCode::OK:
      return 200;
    case StatusCode::CANCELLED:
      return 499;
    case StatusCode::UNKNOWN:
      return 500;
    case StatusCode::INVALID_ARGUMENT:
      return 400;
    case StatusCode::DEADLINE_EXCEEDED:
      return 504;
    case StatusCode::NOT_FOUND:
      return 404;
    case StatusCode::ALREADY_EXISTS:
      return 409;
    case StatusCode::PERMISSION_DENIED:
      return 403;
    case StatusCode::RESOURCE_EXHAUSTED:
      return 429;
    case StatusCode::FAILED_PRECONDITION:
      return 400;
    case StatusCode::ABORTED:
      return 409;
    case StatusCode::OUT_OF_RANGE:
      return 400;
    case StatusCode::UNIMPLEMENTED:
      return 501;
    case StatusCode::INTERNAL:
      return 500;
    case StatusCode::UNAVAILABLE:
      return 503;
    case StatusCode::DATA_LOSS:
      return 500;
    case StatusCode::UNAUTHENTICATED:
      return 401;
    default:
      return 500;
  }
}

bool CheckStatus(const Status &status) {
  int code = HttpCode(status.error_code());
  if ((code == 200) || (code >= 500)) {
    return true;  // we fail open on 5xx errors from mixer
  }
  return false;
}

}  // namespace Utils
}  // namespace Http
}  // namespace Envoy
