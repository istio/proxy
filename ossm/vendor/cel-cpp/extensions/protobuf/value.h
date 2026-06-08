// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Utilities for wrapping and unwrapping cel::Values representing protobuf
// message types.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_

#include <type_traits>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

// Adapt a protobuf message to a cel::Value.
//
// Handles unwrapping message types with special meanings in CEL (WKTs).
//
// T value must be a protobuf message class.
template <typename T>
std::enable_if_t<std::is_base_of_v<google::protobuf::Message, absl::remove_cvref_t<T>>,
                 absl::StatusOr<Value>>
ProtoMessageToValue(T&& value,
                    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena) {
  return Value::FromMessage(std::forward<T>(value), descriptor_pool,
                            message_factory, arena);
}

inline absl::Status ProtoMessageFromValue(const Value& value,
                                          google::protobuf::Message& dest_message) {
  const auto* dest_descriptor = dest_message.GetDescriptor();
  const google::protobuf::Message* src_message = nullptr;
  if (auto legacy_struct_value =
          cel::common_internal::AsLegacyStructValue(value);
      legacy_struct_value) {
    src_message = legacy_struct_value->message_ptr();
  }
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    src_message = cel::to_address(*parsed_message_value);
  }
  if (src_message != nullptr) {
    const auto* src_descriptor = src_message->GetDescriptor();
    if (dest_descriptor == src_descriptor) {
      dest_message.CopyFrom(*src_message);
      return absl::OkStatus();
    }
    if (dest_descriptor->full_name() == src_descriptor->full_name()) {
      absl::Cord serialized;
      if (!src_message->SerializePartialToCord(&serialized)) {
        return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                               src_descriptor->full_name()));
      }
      if (!dest_message.ParsePartialFromCord(serialized)) {
        return absl::UnknownError(absl::StrCat("failed to parse message: ",
                                               dest_descriptor->full_name()));
      }
      return absl::OkStatus();
    }
  }
  return TypeConversionError(value.GetRuntimeType(),
                             MessageType(dest_descriptor))
      .NativeValue();
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_VALUE_H_
