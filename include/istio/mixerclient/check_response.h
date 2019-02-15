/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#ifndef ISTIO_MIXERCLIENT_CHECK_RESPONSE_H
#define ISTIO_MIXERCLIENT_CHECK_RESPONSE_H

#include "google/protobuf/stubs/status.h"
#include "mixer/v1/mixer.pb.h"

namespace istio {
namespace mixerclient {

// The CheckResponseInfo exposes policy and quota check details to the check
// callbacks.
class CheckResponseInfo {
 public:
  virtual ~CheckResponseInfo(){};

  virtual const ::google::protobuf::util::Status& status() const = 0;

  virtual const ::istio::mixer::v1::RouteDirective& routeDirective() const = 0;
};

}  // namespace mixerclient
}  // namespace istio

#endif  // ISTIO_MIXERCLIENT_CHECK_RESPONSE_H
