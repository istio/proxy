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
  LocalAttributes(const Attributes& inbound, const Attributes& outbound,
                  const Attributes& forward)
      : inbound(inbound), outbound(outbound), forward(forward) {}

  // local inbound attributes
  const Attributes inbound;

  // local outbound attributes
  const Attributes outbound;

  // local forward attributes
  const Attributes forward;
};

// LocalNode are used to extract information from envoy Node.
struct LocalNode {
  std::string ns;
  std::string ip;
  std::string uid;
};

std::unique_ptr<const LocalAttributes> CreateLocalAttributes(
    const LocalNode& local);

}  // namespace utils
}  // namespace istio

#endif  // ISTIO_UTILS_LOCAL_ATTRIBUTES_H
