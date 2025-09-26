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
#include <new>
#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

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

absl::Status ErrorValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);
  ABSL_DCHECK(*this);

  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status ErrorValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
  ABSL_DCHECK(*this);

  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status ErrorValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(*this);

  *result = FalseValue();
  return absl::OkStatus();
}

ErrorValue ErrorValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (arena_ == nullptr || arena_ != arena) {
    return ErrorValue(arena,
                      google::protobuf::Arena::Create<absl::Status>(arena, ToStatus()));
  }
  return *this;
}

absl::Status ErrorValue::ToStatus() const& {
  ABSL_DCHECK(*this);

  if (arena_ == nullptr) {
    return *std::launder(
        reinterpret_cast<const absl::Status*>(&status_.val[0]));
  }
  return *status_.ptr;
}

absl::Status ErrorValue::ToStatus() && {
  ABSL_DCHECK(*this);

  if (arena_ == nullptr) {
    return std::move(
        *std::launder(reinterpret_cast<absl::Status*>(&status_.val[0])));
  }
  return *status_.ptr;
}

ErrorValue::operator bool() const {
  if (arena_ == nullptr) {
    return !std::launder(reinterpret_cast<const absl::Status*>(&status_.val[0]))
                ->ok();
  }
  return status_.ptr != nullptr && !status_.ptr->ok();
}

void swap(ErrorValue& lhs, ErrorValue& rhs) noexcept {
  ErrorValue tmp(std::move(lhs));
  lhs = std::move(rhs);
  rhs = std::move(tmp);
}

}  // namespace cel
