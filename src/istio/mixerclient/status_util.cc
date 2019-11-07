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

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace istio {
namespace mixerclient {

static constexpr absl::string_view TIMEOUT_MESSAGE{"upstream request timeout"};
static constexpr absl::string_view SEND_ERROR_MESSAGE{
    "upstream connect error or disconnect/reset before headers"};

TransportResult TransportStatus(
    const ::google::protobuf::util::Status &status) {
  if (status.ok()) {
    return TransportResult::SUCCESS;
  }

  if (::google::protobuf::util::error::Code::UNAVAILABLE ==
      status.error_code()) {
    absl::string_view error_message{status.error_message().data(),
                                    static_cast<absl::string_view::size_type>(
                                        status.error_message().length())};
    if (absl::StartsWith(error_message, TIMEOUT_MESSAGE)) {
      return TransportResult::RESPONSE_TIMEOUT;
    }
    if (absl::StartsWith(error_message, SEND_ERROR_MESSAGE)) {
      return TransportResult::SEND_ERROR;
    }
  }

  return TransportResult::OTHER;
}
}  // namespace mixerclient
}  // namespace istio
