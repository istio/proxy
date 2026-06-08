// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_BIND_PROTO_TO_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_BIND_PROTO_TO_ACTIVATION_H_

#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/casting.h"
#include "common/value.h"
#include "extensions/protobuf/value.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

// Option for handling unset fields on the context proto.
enum class BindProtoUnsetFieldBehavior {
  // Bind the message defined default or zero value.
  kBindDefaultValue,
  // Skip binding unset fields, no value is bound for the corresponding
  // variable.
  kSkip
};

namespace protobuf_internal {

// Implements binding provided the context message has already
// been adapted to a suitable struct value.
absl::Status BindProtoToActivation(
    const google::protobuf::Descriptor& descriptor, const StructValue& struct_value,
    BindProtoUnsetFieldBehavior unset_field_behavior,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Activation* absl_nonnull activation);

}  // namespace protobuf_internal

// Utility method, that takes a protobuf Message and interprets it as a
// namespace, binding its fields to Activation. This is often referred to as a
// context message.
//
// Field names and values become respective names and values of parameters
// bound to the Activation object.
// Example:
// Assume we have a protobuf message of type:
// message Person {
//   int age = 1;
//   string name = 2;
// }
//
// The sample code snippet will look as follows:
//
//   Person person;
//   person.set_name("John Doe");
//   person.age(42);
//
//   CEL_RETURN_IF_ERROR(BindProtoToActivation(person, value_factory,
//   activation));
//
// After this snippet, activation will have two parameters bound:
//  "name", with string value of "John Doe"
//  "age", with int value of 42.
//
// The default behavior for unset fields is to skip them. E.g. if the name field
// is not set on the Person message, it will not be bound in to the activation.
// BindProtoUnsetFieldBehavior::kBindDefault, will bind the cc proto api default
// for the field (either an explicit default value or a type specific default).
//
// For repeated fields, an unset field is bound as an empty list.
template <typename T>
absl::Status BindProtoToActivation(
    const T& context, BindProtoUnsetFieldBehavior unset_field_behavior,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Activation* absl_nonnull activation) {
  static_assert(std::is_base_of_v<google::protobuf::Message, T>);
  // TODO(uncreated-issue/68): for simplicity, just convert the whole message to a
  // struct value. For performance, may be better to convert members as needed.
  CEL_ASSIGN_OR_RETURN(
      Value parent,
      ProtoMessageToValue(context, descriptor_pool, message_factory, arena));

  if (!InstanceOf<StructValue>(parent)) {
    return absl::InvalidArgumentError(
        absl::StrCat("context is a well-known type: ", context.GetTypeName()));
  }
  const StructValue& struct_value = Cast<StructValue>(parent);

  const google::protobuf::Descriptor* descriptor = context.GetDescriptor();

  if (descriptor == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("context missing descriptor: ", context.GetTypeName()));
  }

  return protobuf_internal::BindProtoToActivation(
      *descriptor, struct_value, unset_field_behavior, descriptor_pool,
      message_factory, arena, activation);
}
template <typename T>
absl::Status BindProtoToActivation(
    const T& context,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Activation* absl_nonnull activation) {
  return BindProtoToActivation(context, BindProtoUnsetFieldBehavior::kSkip,
                               descriptor_pool, message_factory, arena,
                               activation);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_BIND_PROTO_TO_ACTIVATION_H_
