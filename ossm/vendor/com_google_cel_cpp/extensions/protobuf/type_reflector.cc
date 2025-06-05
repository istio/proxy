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

#include "extensions/protobuf/type_reflector.h"

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/struct_value_builder.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {
absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
ProtoTypeReflector::NewStructValueBuilder(ValueFactory& value_factory,
                                          const StructType& type) const {
  auto status_or_builder = common_internal::NewStructValueBuilder(
      value_factory.GetMemoryManager().arena(), descriptor_pool(),
      message_factory(), type.name());
  if (!status_or_builder.ok() && absl::IsNotFound(status_or_builder.status())) {
    return nullptr;
  }
  return status_or_builder;
}

absl::StatusOr<absl::optional<Value>> ProtoTypeReflector::DeserializeValueImpl(
    ValueFactory& value_factory, absl::string_view type_url,
    const absl::Cord& value) const {
  absl::string_view type_name;
  if (!ParseTypeUrl(type_url, &type_name)) {
    return absl::InvalidArgumentError("invalid type URL");
  }
  const auto* descriptor = descriptor_pool()->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return absl::nullopt;
  }
  const auto* prototype = message_factory()->GetPrototype(descriptor);
  if (prototype == nullptr) {
    return absl::nullopt;
  }
  absl::Nullable<google::protobuf::Arena*> arena =
      value_factory.GetMemoryManager().arena();
  auto message = WrapShared(prototype->New(arena), arena);
  if (!message->ParsePartialFromCord(value)) {
    return absl::UnknownError(
        absl::StrCat("failed to parse message: ", descriptor->full_name()));
  }
  return Value::Message(message, descriptor_pool(), message_factory());
}

}  // namespace cel::extensions
