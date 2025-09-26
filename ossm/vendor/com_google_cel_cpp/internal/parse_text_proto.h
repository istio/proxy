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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PARSE_TEXT_PROTO_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PARSE_TEXT_PROTO_H_

#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "internal/message_type_name.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"

namespace cel::internal {

// `GeneratedParseTextProto` parses the text format protocol buffer message as
// the message with the same name as `T`, looked up in the provided descriptor
// pool, returning as the generated message. This works regardless of whether
// all messages are built with the lite runtime or not.
template <typename T>
std::enable_if_t<std::is_base_of_v<google::protobuf::Message, T>, T* absl_nonnull>
GeneratedParseTextProto(
    google::protobuf::Arena* absl_nonnull arena, absl::string_view text,
    const google::protobuf::DescriptorPool* absl_nonnull pool =
        GetTestingDescriptorPool(),
    google::protobuf::MessageFactory* absl_nonnull factory = GetTestingMessageFactory()) {
  // Full runtime.
  const auto* descriptor = ABSL_DIE_IF_NULL(  // Crash OK
      pool->FindMessageTypeByName(MessageTypeNameFor<T>()));
  const auto* dynamic_message_prototype =
      ABSL_DIE_IF_NULL(factory->GetPrototype(descriptor));  // Crash OK
  auto* dynamic_message = dynamic_message_prototype->New(arena);
  ABSL_CHECK(  // Crash OK
      google::protobuf::TextFormat::ParseFromString(text, dynamic_message));
  if (auto* generated_message = google::protobuf::DynamicCastMessage<T>(dynamic_message);
      generated_message != nullptr) {
    // Same thing, no need to serialize and parse.
    return generated_message;
  }
  auto* message = google::protobuf::Arena::Create<T>(arena);
  absl::Cord serialized_message;
  ABSL_CHECK(  // Crash OK
      dynamic_message->SerializeToCord(&serialized_message));
  ABSL_CHECK(message->ParseFromCord(serialized_message));  // Crash OK
  return message;
}

// `GeneratedParseTextProto` parses the text format protocol buffer message as
// the message with the same name as `T`, looked up in the provided descriptor
// pool, returning as the generated message. This works regardless of whether
// all messages are built with the lite runtime or not.
template <typename T>
std::enable_if_t<
    std::conjunction_v<std::is_base_of<google::protobuf::MessageLite, T>,
                       std::negation<std::is_base_of<google::protobuf::Message, T>>>,
    T* absl_nonnull>
GeneratedParseTextProto(
    google::protobuf::Arena* absl_nonnull arena, absl::string_view text,
    const google::protobuf::DescriptorPool* absl_nonnull pool =
        GetTestingDescriptorPool(),
    google::protobuf::MessageFactory* absl_nonnull factory = GetTestingMessageFactory()) {
  // Lite runtime.
  const auto* descriptor = ABSL_DIE_IF_NULL(  // Crash OK
      pool->FindMessageTypeByName(MessageTypeNameFor<T>()));
  const auto* dynamic_message_prototype =
      ABSL_DIE_IF_NULL(factory->GetPrototype(descriptor));  // Crash OK
  auto* dynamic_message = dynamic_message_prototype->New(arena);
  ABSL_CHECK(  // Crash OK
      google::protobuf::TextFormat::ParseFromString(text, dynamic_message));
  auto* message = google::protobuf::Arena::Create<T>(arena);
  absl::Cord serialized_message;
  ABSL_CHECK(  // Crash OK
      dynamic_message->SerializeToCord(&serialized_message));
  ABSL_CHECK(message->ParseFromCord(serialized_message));  // Crash OK
  return message;
}

// `DynamicParseTextProto` parses the text format protocol buffer message as the
// dynamic message with the same name as `T`, looked up in the provided
// descriptor pool, returning the dynamic message.
template <typename T>
google::protobuf::Message* absl_nonnull DynamicParseTextProto(
    google::protobuf::Arena* absl_nonnull arena, absl::string_view text,
    const google::protobuf::DescriptorPool* absl_nonnull pool =
        GetTestingDescriptorPool(),
    google::protobuf::MessageFactory* absl_nonnull factory = GetTestingMessageFactory()) {
  static_assert(std::is_base_of_v<google::protobuf::MessageLite, T>);
  const auto* descriptor = ABSL_DIE_IF_NULL(  // Crash OK
      pool->FindMessageTypeByName(MessageTypeNameFor<T>()));
  const auto* dynamic_message_prototype =
      ABSL_DIE_IF_NULL(factory->GetPrototype(descriptor));  // Crash OK
  auto* dynamic_message = dynamic_message_prototype->New(arena);
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(  // Crash OK
      text, cel::to_address(dynamic_message)));
  return dynamic_message;
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PARSE_TEXT_PROTO_H_
