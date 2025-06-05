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

// `ParsedListValue` represents values of the primitive `list` type.
// `ParsedListValueView` is a non-owning view of `ParsedListValue`.
// `ParsedListValueInterface` is the abstract base class of implementations.
// `ParsedListValue` and `ParsedListValueView` act as smart pointers to
// `ParsedListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/list_value_interface.h"
#include "common/values/values.h"

namespace cel {

class Value;
class ParsedListValueInterface;
class ParsedListValueInterfaceIterator;
class ParsedListValue;
class ValueManager;

// `Is` checks whether `lhs` and `rhs` have the same identity.
bool Is(const ParsedListValue& lhs, const ParsedListValue& rhs);

class ParsedListValueInterface : public ListValueInterface {
 public:
  using alternative_type = ParsedListValue;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const override;

  virtual absl::Status Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const;

  bool IsZeroValue() const { return IsEmpty(); }

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  // Returns a view of the element at index `index`. If the underlying
  // implementation cannot directly return a view of a value, the value will be
  // stored in `scratch`, and the returned view will be that of `scratch`.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachWithIndexCallback callback) const;

  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  virtual absl::Status Contains(ValueManager& value_manager, const Value& other,
                                Value& result) const;

  virtual ParsedListValue Clone(ArenaAllocator<> allocator) const = 0;

 protected:
  friend class ParsedListValueInterfaceIterator;

  virtual absl::Status GetImpl(ValueManager& value_manager, size_t index,
                               Value& result) const = 0;
};

class ParsedListValue {
 public:
  using interface_type = ParsedListValueInterface;

  static constexpr ValueKind kKind = ParsedListValueInterface::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedListValue(Shared<const ParsedListValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  ParsedListValue();
  ParsedListValue(const ParsedListValue&) = default;
  ParsedListValue(ParsedListValue&&) = default;
  ParsedListValue& operator=(const ParsedListValue&) = default;
  ParsedListValue& operator=(ParsedListValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const {
    return interface_->SerializeTo(converter, value);
  }

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const {
    return interface_->ConvertToJson(converter);
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const {
    return interface_->ConvertToJsonArray(converter);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  ParsedListValue Clone(Allocator<> allocator) const;

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;

  void swap(ParsedListValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<ParsedListValue>;
  friend bool Is(const ParsedListValue& lhs, const ParsedListValue& rhs);

  Shared<const ParsedListValueInterface> interface_;
};

inline void swap(ParsedListValue& lhs, ParsedListValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedListValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ParsedListValue> final {
  static NativeTypeId Id(const ParsedListValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const ParsedListValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<ParsedListValue, T>>,
                               std::is_base_of<ParsedListValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<ParsedListValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<ParsedListValue>::SkipDestructor(type);
  }
};

inline bool Is(const ParsedListValue& lhs, const ParsedListValue& rhs) {
  return lhs.interface_.operator->() == rhs.interface_.operator->();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
