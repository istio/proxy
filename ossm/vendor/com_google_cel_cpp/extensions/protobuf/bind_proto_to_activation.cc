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

#include "extensions/protobuf/bind_proto_to_activation.h"

#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/activation.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

namespace {

using ::google::protobuf::Descriptor;

absl::StatusOr<bool> ShouldBindField(
    const google::protobuf::FieldDescriptor* field_desc, const StructValue& struct_value,
    BindProtoUnsetFieldBehavior unset_field_behavior) {
  if (unset_field_behavior == BindProtoUnsetFieldBehavior::kBindDefaultValue ||
      field_desc->is_repeated()) {
    return true;
  }
  return struct_value.HasFieldByNumber(field_desc->number());
}

absl::StatusOr<Value> GetFieldValue(
    const google::protobuf::FieldDescriptor* field_desc, const StructValue& struct_value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  // Special case unset any.
  if (field_desc->cpp_type() == google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE &&
      field_desc->message_type()->well_known_type() ==
          Descriptor::WELLKNOWNTYPE_ANY) {
    CEL_ASSIGN_OR_RETURN(bool present,
                         struct_value.HasFieldByNumber(field_desc->number()));
    if (!present) {
      return NullValue();
    }
  }

  return struct_value.GetFieldByNumber(field_desc->number(), descriptor_pool,
                                       message_factory, arena);
}

}  // namespace

absl::Status BindProtoToActivation(
    const Descriptor& descriptor, const StructValue& struct_value,
    BindProtoUnsetFieldBehavior unset_field_behavior,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Activation* absl_nonnull activation) {
  for (int i = 0; i < descriptor.field_count(); i++) {
    const google::protobuf::FieldDescriptor* field_desc = descriptor.field(i);
    CEL_ASSIGN_OR_RETURN(
        bool should_bind,
        ShouldBindField(field_desc, struct_value, unset_field_behavior));
    if (!should_bind) {
      continue;
    }

    CEL_ASSIGN_OR_RETURN(
        Value field, GetFieldValue(field_desc, struct_value, descriptor_pool,
                                   message_factory, arena));

    activation->InsertOrAssignValue(field_desc->name(), std::move(field));
  }

  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
