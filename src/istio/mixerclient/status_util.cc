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

#include "src/istio/mixerclient/status_util.h"

namespace istio {
namespace mixerclient {

static ::google::protobuf::StringPiece TIMEOUT_MESSAGE(
    "upstream request timeout");
static ::google::protobuf::StringPiece SEND_ERROR_MESSAGE(
    "upstream connect error or disconnect/reset before headers");

TransportResult TransportStatus(
    const ::google::protobuf::util::Status &status) {
  if (status.ok()) {
    return TransportResult::SUCCESS;
  }

  if (::google::protobuf::util::error::Code::UNAVAILABLE ==
      status.error_code()) {
    if (TIMEOUT_MESSAGE == status.error_message()) {
      return TransportResult::RESPONSE_TIMEOUT;
    }
    if (SEND_ERROR_MESSAGE == status.error_message()) {
      return TransportResult::SEND_ERROR;
    }
  }

  return TransportResult::OTHER;
}
}  // namespace mixerclient
}  // namespace istio
