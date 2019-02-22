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

#include "google/protobuf/stubs/status.h"

namespace istio {
namespace mixerclient {

enum class TransportResult {
  SUCCESS,           // Response received
  SEND_ERROR,        // Cannot connect to peer or send request to peer.
  RESPONSE_TIMEOUT,  // Connected to peer and sent request, but didn't receive a
  // response in time.
      OTHER              // Something else went wrong
};

extern TransportResult TransportStatus(const ::google::protobuf::util::Status &status);

}
}