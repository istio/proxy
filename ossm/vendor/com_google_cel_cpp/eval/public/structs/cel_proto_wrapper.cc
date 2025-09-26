// Copyright 2021 Google LLC
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

#include "eval/public/structs/cel_proto_wrapper.h"

#include "absl/types/optional.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrap_util.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;
using ::google::protobuf::Descriptor;
using ::google::protobuf::Message;

}  // namespace

CelValue CelProtoWrapper::InternalWrapMessage(const Message* message) {
  return CelValue::CreateMessageWrapper(
      MessageWrapper(message, &GetGenericProtoTypeInfoInstance()));
}

// CreateMessage creates CelValue from google::protobuf::Message.
// As some of CEL basic types are subclassing google::protobuf::Message,
// this method contains type checking and downcasts.
CelValue CelProtoWrapper::CreateMessage(const Message* value, Arena* arena) {
  return internal::UnwrapMessageToValue(value, &InternalWrapMessage, arena);
}

absl::optional<CelValue> CelProtoWrapper::MaybeWrapValue(
    const Descriptor* descriptor, google::protobuf::MessageFactory* factory,
    const CelValue& value, Arena* arena) {
  const Message* msg =
      internal::MaybeWrapValueToMessage(descriptor, factory, value, arena);
  if (msg != nullptr) {
    return InternalWrapMessage(msg);
  } else {
    return absl::nullopt;
  }
}

}  // namespace google::api::expr::runtime
