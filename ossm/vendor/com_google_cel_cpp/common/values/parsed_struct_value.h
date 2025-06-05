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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/struct_value_interface.h"
#include "runtime/runtime_options.h"

namespace cel {

class ParsedStructValueInterface;
class ParsedStructValue;
class Value;
class ValueManager;

class ParsedStructValueInterface : public StructValueInterface {
 public:
  using alternative_type = ParsedStructValue;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  virtual bool IsZeroValue() const = 0;

  virtual absl::Status GetFieldByName(
      ValueManager& value_manager, absl::string_view name, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const = 0;

  virtual absl::Status GetFieldByNumber(
      ValueManager& value_manager, int64_t number, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

  virtual absl::Status ForEachField(ValueManager& value_manager,
                                    ForEachFieldCallback callback) const = 0;

  virtual absl::StatusOr<int> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test, Value& result) const;

  virtual ParsedStructValue Clone(ArenaAllocator<> allocator) const = 0;

 protected:
  virtual absl::Status EqualImpl(ValueManager& value_manager,
                                 const ParsedStructValue& other,
                                 Value& result) const;
};

class ParsedStructValue {
 public:
  using interface_type = ParsedStructValueInterface;

  static constexpr ValueKind kKind = ParsedStructValueInterface::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedStructValue(Shared<const ParsedStructValueInterface> interface)
      : interface_(std::move(interface)) {}

  ParsedStructValue() = default;
  ParsedStructValue(const ParsedStructValue&) = default;
  ParsedStructValue(ParsedStructValue&&) = default;
  ParsedStructValue& operator=(const ParsedStructValue&) = default;
  ParsedStructValue& operator=(ParsedStructValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const { return interface_->GetRuntimeType(); }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const {
    return interface_->SerializeTo(converter, value);
  }

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const {
    return interface_->ConvertToJson(converter);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  ParsedStructValue Clone(Allocator<> allocator) const;

  void swap(ParsedStructValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  absl::Status GetFieldByName(ValueManager& value_manager,
                              absl::string_view name, Value& result,
                              ProtoWrapperTypeOptions unboxing_options) const;

  absl::Status GetFieldByNumber(ValueManager& value_manager, int64_t number,
                                Value& result,
                                ProtoWrapperTypeOptions unboxing_options) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
    return interface_->HasFieldByName(name);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
    return interface_->HasFieldByNumber(number);
  }

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<ParsedStructValue>;

  Shared<const ParsedStructValueInterface> interface_;
};

inline void swap(ParsedStructValue& lhs, ParsedStructValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedStructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ParsedStructValue> final {
  static NativeTypeId Id(const ParsedStructValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const ParsedStructValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<
           std::conjunction_v<std::negation<std::is_same<ParsedStructValue, T>>,
                              std::is_base_of<ParsedStructValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<ParsedStructValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<ParsedStructValue>::SkipDestructor(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_STRUCT_VALUE_H_
