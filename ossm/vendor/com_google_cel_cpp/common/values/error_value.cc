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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace {

std::string ErrorDebugString(const absl::Status& value) {
  ABSL_DCHECK(!value.ok()) << "use of moved-from ErrorValue";
  return value.ToString(absl::StatusToStringMode::kWithEverything);
}

const absl::Status& DefaultErrorValue() {
  static const absl::NoDestructor<absl::Status> value(
      absl::UnknownError("unknown error"));
  return *value;
}

}  // namespace

ErrorValue::ErrorValue() : ErrorValue(DefaultErrorValue()) {}

ErrorValue NoSuchFieldError(absl::string_view field) {
  return ErrorValue(absl::NotFoundError(
      absl::StrCat("no_such_field", field.empty() ? "" : " : ", field)));
}

ErrorValue NoSuchKeyError(absl::string_view key) {
  return ErrorValue(
      absl::NotFoundError(absl::StrCat("Key not found in map : ", key)));
}

ErrorValue NoSuchTypeError(absl::string_view type) {
  return ErrorValue(
      absl::NotFoundError(absl::StrCat("type not found: ", type)));
}

ErrorValue DuplicateKeyError() {
  return ErrorValue(absl::AlreadyExistsError("duplicate key in map"));
}

ErrorValue TypeConversionError(absl::string_view from, absl::string_view to) {
  return ErrorValue(absl::InvalidArgumentError(
      absl::StrCat("type conversion error from '", from, "' to '", to, "'")));
}

ErrorValue TypeConversionError(const Type& from, const Type& to) {
  return TypeConversionError(from.DebugString(), to.DebugString());
}

ErrorValue IndexOutOfBoundsError(size_t index) {
  return ErrorValue(
      absl::InvalidArgumentError(absl::StrCat("index out of bounds: ", index)));
}

ErrorValue IndexOutOfBoundsError(ptrdiff_t index) {
  return ErrorValue(
      absl::InvalidArgumentError(absl::StrCat("index out of bounds: ", index)));
}

bool IsNoSuchField(const ErrorValue& value) {
  return absl::IsNotFound(value.NativeValue()) &&
         absl::StartsWith(value.NativeValue().message(), "no_such_field");
}

bool IsNoSuchKey(const ErrorValue& value) {
  return absl::IsNotFound(value.NativeValue()) &&
         absl::StartsWith(value.NativeValue().message(),
                          "Key not found in map");
}

std::string ErrorValue::DebugString() const {
  return ErrorDebugString(NativeValue());
}

absl::Status ErrorValue::SerializeTo(AnyToJsonConverter&, absl::Cord&) const {
  ABSL_DCHECK(*this);
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> ErrorValue::ConvertToJson(AnyToJsonConverter&) const {
  ABSL_DCHECK(*this);
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status ErrorValue::Equal(ValueManager&, const Value&,
                               Value& result) const {
  ABSL_DCHECK(*this);
  result = BoolValue{false};
  return absl::OkStatus();
}

ErrorValue ErrorValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr) {
    return ErrorValue(absl::visit(
        absl::Overload(
            [arena](const absl::Status& status) -> ArenaStatus {
              return ArenaStatus{
                  arena, google::protobuf::Arena::Create<absl::Status>(arena, status)};
            },
            [arena](const ArenaStatus& status) -> ArenaStatus {
              if (status.first != nullptr && status.first != arena) {
                return ArenaStatus{arena, google::protobuf::Arena::Create<absl::Status>(
                                              arena, *status.second)};
              }
              return status;
            }),
        variant_));
  }
  return ErrorValue(NativeValue());
}

absl::Status ErrorValue::NativeValue() const& {
  ABSL_DCHECK(*this);
  return absl::visit(absl::Overload(
                         [](const absl::Status& status) -> const absl::Status& {
                           return status;
                         },
                         [](const ArenaStatus& status) -> const absl::Status& {
                           return *status.second;
                         }),
                     variant_);
}

absl::Status ErrorValue::NativeValue() && {
  ABSL_DCHECK(*this);
  return absl::visit(absl::Overload(
                         [](absl::Status&& status) -> absl::Status {
                           return std::move(status);
                         },
                         [](const ArenaStatus& status) -> absl::Status {
                           return *status.second;
                         }),
                     std::move(variant_));
}

ErrorValue::operator bool() const {
  return absl::visit(
      absl::Overload(
          [](const absl::Status& status) -> bool { return !status.ok(); },
          [](const ArenaStatus& status) -> bool {
            return !status.second->ok();
          }),
      variant_);
}

void swap(ErrorValue& lhs, ErrorValue& rhs) noexcept {
  lhs.variant_.swap(rhs.variant_);
}

}  // namespace cel
