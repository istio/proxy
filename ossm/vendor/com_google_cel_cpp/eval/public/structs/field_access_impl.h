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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_FIELD_ACCESS_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_FIELD_ACCESS_IMPL_H_

#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/protobuf_value_factory.h"

namespace google::api::expr::runtime::internal {

// Creates CelValue from singular message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// options Option to enable treating unset wrapper type fields as null.
// arena Arena object to allocate result on, if needed.
// result pointer to CelValue to store the result in.
absl::StatusOr<CelValue> CreateValueFromSingleField(
    const google::protobuf::Message* msg, const google::protobuf::FieldDescriptor* desc,
    ProtoWrapperTypeOptions options, const ProtobufValueFactory& factory,
    google::protobuf::Arena* arena);

// Creates CelValue from repeated message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// arena Arena object to allocate result on, if needed.
// index position in the repeated field.
absl::StatusOr<CelValue> CreateValueFromRepeatedField(
    const google::protobuf::Message* msg, const google::protobuf::FieldDescriptor* desc, int index,
    const ProtobufValueFactory& factory, google::protobuf::Arena* arena);

// Creates CelValue from map message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// value_ref pointer to map value.
// arena Arena object to allocate result on, if needed.
// TODO(uncreated-issue/7): This should be inlined into the FieldBackedMap
// implementation.
absl::StatusOr<CelValue> CreateValueFromMapValue(
    const google::protobuf::Message* msg, const google::protobuf::FieldDescriptor* desc,
    const google::protobuf::MapValueConstRef* value_ref,
    const ProtobufValueFactory& factory, google::protobuf::Arena* arena);

// Assigns content of CelValue to singular message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// arena Arena to perform allocations, if necessary, when setting the field.
absl::Status SetValueToSingleField(const CelValue& value,
                                   const google::protobuf::FieldDescriptor* desc,
                                   google::protobuf::Message* msg, google::protobuf::Arena* arena);

// Adds content of CelValue to repeated message field.
// Returns status of the operation.
// msg Message containing the field.
// desc Descriptor of the field to access.
// arena Arena to perform allocations, if necessary, when adding the value.
absl::Status AddValueToRepeatedField(const CelValue& value,
                                     const google::protobuf::FieldDescriptor* desc,
                                     google::protobuf::Message* msg,
                                     google::protobuf::Arena* arena);

}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_FIELD_ACCESS_IMPL_H_
