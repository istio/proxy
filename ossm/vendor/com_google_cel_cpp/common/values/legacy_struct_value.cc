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

#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/internal/message_wrapper.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel::common_internal {

StructType LegacyStructValue::GetRuntimeType() const {
  if ((message_ptr_ & ::cel::base_internal::kMessageWrapperTagMask) ==
      ::cel::base_internal::kMessageWrapperTagMessageValue) {
    return MessageType(
        google::protobuf::DownCastMessage<google::protobuf::Message>(
            reinterpret_cast<const google::protobuf::MessageLite*>(
                message_ptr_ & ::cel::base_internal::kMessageWrapperPtrMask))
            ->GetDescriptor());
  }
  return common_internal::MakeBasicStructType(GetTypeName());
}

bool IsLegacyStructValue(const Value& value) {
  return absl::holds_alternative<LegacyStructValue>(value.variant_);
}

LegacyStructValue GetLegacyStructValue(const Value& value) {
  ABSL_DCHECK(IsLegacyStructValue(value));
  return absl::get<LegacyStructValue>(value.variant_);
}

absl::optional<LegacyStructValue> AsLegacyStructValue(const Value& value) {
  if (IsLegacyStructValue(value)) {
    return GetLegacyStructValue(value);
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal
