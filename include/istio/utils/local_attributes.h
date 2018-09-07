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

#ifndef ISTIO_UTILS_LOCAL_ATTRIBUTES_H
#define ISTIO_UTILS_LOCAL_ATTRIBUTES_H

#include "mixer/v1/attributes.pb.h"

using ::istio::mixer::v1::Attributes;

namespace istio {
namespace utils {

struct LocalAttributes {
  // local inbound attributes
  Attributes inbound;

  // local outbound attributes
  Attributes outbound;

  // local forward attributes
  Attributes forward;
};

// LocalNode is abstract information about the node from Mixer's perspective.
struct LocalNode {
  // like kubernetes://podname.namespace
  std::string uid;

  // namespace
  std::string ns;
};

void CreateLocalAttributes(const LocalNode& local,
                           LocalAttributes* local_attributes);

}  // namespace utils
}  // namespace istio

#endif  // ISTIO_UTILS_LOCAL_ATTRIBUTES_H
