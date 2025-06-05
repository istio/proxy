// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAP_UTIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAP_UTIL_H_

#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime::internal {

// UnwrapValue creates CelValue from google::protobuf::Message.
// As some of CEL basic types are subclassing google::protobuf::Message,
// this method contains type checking and downcasts.
CelValue UnwrapMessageToValue(const google::protobuf::Message* value,
                              const ProtobufValueFactory& factory,
                              google::protobuf::Arena* arena);

// MaybeWrapValue attempts to wrap the input value in a proto message with
// the given type_name. If the value can be wrapped, it is returned as a
// protobuf message. Otherwise, the result will be nullptr.
//
// This method is the complement to MaybeUnwrapValue which may unwrap a protobuf
// message to native CelValue representation during a protobuf field read.
// Just as CreateMessage should only be used when reading protobuf values,
// MaybeWrapValue should only be used when assigning protobuf fields.
const google::protobuf::Message* MaybeWrapValueToMessage(
    const google::protobuf::Descriptor* descriptor, google::protobuf::MessageFactory* factory,
    const CelValue& value, google::protobuf::Arena* arena);

}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_WRAP_UTIL_H_
