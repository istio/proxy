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

#include <string>

#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/type.h"
#include "common/types/types.h"

namespace cel {

absl::string_view StructType::name() const {
  ABSL_DCHECK(*this);
  return absl::visit(
      absl::Overload([](absl::monostate) { return absl::string_view(); },
                     [](const common_internal::BasicStructType& alt) {
                       return alt.name();
                     },
                     [](const MessageType& alt) { return alt.name(); }),
      variant_);
}

TypeParameters StructType::GetParameters() const {
  ABSL_DCHECK(*this);
  return absl::visit(
      absl::Overload(
          [](absl::monostate) { return TypeParameters(); },
          [](const common_internal::BasicStructType& alt) {
            return alt.GetParameters();
          },
          [](const MessageType& alt) { return alt.GetParameters(); }),
      variant_);
}

std::string StructType::DebugString() const {
  return absl::visit(
      absl::Overload([](absl::monostate) { return std::string(); },
                     [](common_internal::BasicStructType alt) {
                       return alt.DebugString();
                     },
                     [](MessageType alt) { return alt.DebugString(); }),
      variant_);
}

absl::optional<MessageType> StructType::AsMessage() const {
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

MessageType StructType::GetMessage() const {
  ABSL_DCHECK(IsMessage()) << DebugString();
  return absl::get<MessageType>(variant_);
}

common_internal::TypeVariant StructType::ToTypeVariant() const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) { return common_internal::TypeVariant(); },
          [](common_internal::BasicStructType alt) {
            return static_cast<bool>(alt) ? common_internal::TypeVariant(alt)
                                          : common_internal::TypeVariant();
          },
          [](MessageType alt) {
            return static_cast<bool>(alt) ? common_internal::TypeVariant(alt)
                                          : common_internal::TypeVariant();
          }),
      variant_);
}

}  // namespace cel
