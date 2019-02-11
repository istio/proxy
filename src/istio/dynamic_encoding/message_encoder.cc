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
#include <memory>

#include "message_encoder.h"
#include "primitive_encoder.h"
#include "util.h"

#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/stubs/status.h"
#include "google/protobuf/stubs/statusor.h"

namespace istio {
namespace dynamic_encoding {
using ::google::protobuf::util::Status;
using google::protobuf::util::error::INTERNAL;

::google::protobuf::util::StatusOr<absl::any> MessageEncoder::Encode() {
  for (auto it = fields_.begin(); it != fields_.end(); ++it) {
    auto status_or_value = it->first->Encode();
    if (!status_or_value.ok()) {
      return status_or_value.status();
    }
    absl::any value = status_or_value.ValueOrDie();

    PrimitiveEncoder* primEncoder =
        dynamic_cast<PrimitiveEncoder*>(it->first.get());
    if (primEncoder != nullptr) {
      // return Status(INTERNAL, "Could not find fieldEncoder");
      auto status =
          EncodeStaticField(&value, this, primEncoder->GetFieldDescriptor(),
                            primEncoder->GetIndex());
      if (!status.ok()) {
        return status;
      }
      continue;
    }

    MessageEncoder* msgEncoder = dynamic_cast<MessageEncoder*>(it->first.get());
    if (msgEncoder != nullptr) {
      auto status =
          EncodeMessageField(&value, this, it->second, msgEncoder->GetIndex());
      if (!status.ok()) {
        return status;
      }
      continue;
    }
  }
  return absl::any(msg_.get());
}

::google::protobuf::util::StatusOr<std::string> MessageEncoder::EncodeBytes() {
  auto status_or_value = Encode();
  if (!status_or_value.ok()) {
    return status_or_value.status();
  }
  return msg_->message()->SerializeAsString();
}

}  // namespace dynamic_encoding
}  // namespace istio
