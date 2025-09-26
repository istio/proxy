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

#include "common/values/parsed_json_value.h"

#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/value.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace cel::common_internal {

namespace {

using ::cel::well_known_types::AsVariant;
using ::cel::well_known_types::GetValueReflectionOrDie;

google::protobuf::Arena* absl_nonnull MessageArenaOr(
    const google::protobuf::Message* absl_nonnull message,
    google::protobuf::Arena* absl_nonnull or_arena) {
  google::protobuf::Arena* absl_nullable arena = message->GetArena();
  if (arena == nullptr) {
    arena = or_arena;
  }
  return arena;
}

}  // namespace

Value ParsedJsonValue(const google::protobuf::Message* absl_nonnull message,
                      google::protobuf::Arena* absl_nonnull arena) {
  const auto reflection = GetValueReflectionOrDie(message->GetDescriptor());
  const auto kind_case = reflection.GetKindCase(*message);
  switch (kind_case) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return NullValue();
    case google::protobuf::Value::kBoolValue:
      return BoolValue(reflection.GetBoolValue(*message));
    case google::protobuf::Value::kNumberValue:
      return DoubleValue(reflection.GetNumberValue(*message));
    case google::protobuf::Value::kStringValue: {
      std::string scratch;
      return absl::visit(
          absl::Overload(
              [&](absl::string_view string) -> StringValue {
                if (string.empty()) {
                  return StringValue();
                }
                if (string.data() == scratch.data() &&
                    string.size() == scratch.size()) {
                  return StringValue(arena, std::move(scratch));
                } else {
                  return StringValue(
                      Borrower::Arena(MessageArenaOr(message, arena)), string);
                }
              },
              [&](absl::Cord&& cord) -> StringValue {
                if (cord.empty()) {
                  return StringValue();
                }
                return StringValue(std::move(cord));
              }),
          AsVariant(reflection.GetStringValue(*message, scratch)));
    }
    case google::protobuf::Value::kListValue:
      return ParsedJsonListValue(&reflection.GetListValue(*message),
                                 MessageArenaOr(message, arena));
    case google::protobuf::Value::kStructValue:
      return ParsedJsonMapValue(&reflection.GetStructValue(*message),
                                MessageArenaOr(message, arena));
    default:
      return ErrorValue(absl::InvalidArgumentError(
          absl::StrCat("unexpected value kind case: ", kind_case)));
  }
}

}  // namespace cel::common_internal
