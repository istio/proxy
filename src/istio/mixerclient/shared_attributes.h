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

#include "google/protobuf/arena.h"
#include "mixer/v1/attributes.pb.h"

namespace istio {
namespace mixerclient {

/**
 * Attributes shared by the policy/quota check requests and telemetry requests
 * sent to the Mixer server.
 */
class SharedAttributes {
 public:
  SharedAttributes()
      : attributes_(google::protobuf::Arena::CreateMessage<
                    ::istio::mixer::v1::Attributes>(&arena_)) {}

  const ::istio::mixer::v1::Attributes* attributes() const {
    return attributes_;
  }
  ::istio::mixer::v1::Attributes* attributes() { return attributes_; }

  google::protobuf::Arena& arena() { return arena_; }

 private:
  google::protobuf::Arena arena_;
  ::istio::mixer::v1::Attributes* attributes_;
};

typedef std::shared_ptr<SharedAttributes> SharedAttributesSharedPtr;

}  // namespace mixerclient
}  // namespace istio
