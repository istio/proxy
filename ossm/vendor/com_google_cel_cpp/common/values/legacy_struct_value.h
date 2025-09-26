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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_struct_value.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
class LegacyTypeInfoApis;
}

namespace cel {

class Value;

namespace common_internal {

class LegacyStructValue;

// `LegacyStructValue` is a wrapper around the old representation of protocol
// buffer messages in `google::api::expr::runtime::CelValue`. It only supports
// arena allocation.
class LegacyStructValue final
    : private common_internal::StructValueMixin<LegacyStructValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  LegacyStructValue() = default;

  LegacyStructValue(
      const google::protobuf::Message* absl_nullability_unknown message_ptr,
      const google::api::expr::runtime::
          LegacyTypeInfoApis* absl_nullability_unknown legacy_type_info)
      : message_ptr_(message_ptr), legacy_type_info_(legacy_type_info) {}

  LegacyStructValue(const LegacyStructValue&) = default;
  LegacyStructValue& operator=(const LegacyStructValue&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using StructValueMixin::Equal;

  bool IsZeroValue() const;

  absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByName;

  absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using StructValueMixin::GetFieldByNumber;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = CustomStructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(
      ForEachFieldCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
      int* absl_nonnull count) const;
  using StructValueMixin::Qualify;

  const google::protobuf::Message* absl_nullability_unknown message_ptr() const {
    return message_ptr_;
  }

  const google::api::expr::runtime::LegacyTypeInfoApis* absl_nullability_unknown
  legacy_type_info() const {
    return legacy_type_info_;
  }

  friend void swap(LegacyStructValue& lhs, LegacyStructValue& rhs) noexcept {
    using std::swap;
    swap(lhs.message_ptr_, rhs.message_ptr_);
    swap(lhs.legacy_type_info_, rhs.legacy_type_info_);
  }

 private:
  friend class common_internal::ValueMixin<LegacyStructValue>;
  friend class common_internal::StructValueMixin<LegacyStructValue>;

  const google::protobuf::Message* absl_nullability_unknown message_ptr_ = nullptr;
  const google::api::expr::runtime::LegacyTypeInfoApis* absl_nullability_unknown
  legacy_type_info_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const LegacyStructValue& value) {
  return out << value.DebugString();
}

bool IsLegacyStructValue(const Value& value);

LegacyStructValue GetLegacyStructValue(const Value& value);

absl::optional<LegacyStructValue> AsLegacyStructValue(const Value& value);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
