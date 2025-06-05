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
// IWYU pragma: friend "common/values/optional_value.h"

// `OpaqueValue` represents values of the `opaque` type. `OpaqueValueView`
// is a non-owning view of `OpaqueValue`. `OpaqueValueInterface` is the abstract
// base class of implementations. `OpaqueValue` and `OpaqueValueView` act as
// smart pointers to `OpaqueValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/values.h"

namespace cel {

class Value;
class OpaqueValueInterface;
class OpaqueValueInterfaceIterator;
class OpaqueValue;
class TypeFactory;
class ValueManager;

class OpaqueValueInterface : public ValueInterface {
 public:
  using alternative_type = OpaqueValue;

  static constexpr ValueKind kKind = ValueKind::kOpaque;

  ValueKind kind() const final { return kKind; }

  virtual OpaqueType GetRuntimeType() const = 0;

  virtual absl::Status Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const = 0;

  virtual OpaqueValue Clone(ArenaAllocator<> allocator) const = 0;
};

class OpaqueValue {
 public:
  using interface_type = OpaqueValueInterface;

  static constexpr ValueKind kKind = OpaqueValueInterface::kKind;

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            OpaqueValueInterface, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueValue(Shared<T> interface) : interface_(std::move(interface)) {}

  OpaqueValue() = default;
  OpaqueValue(const OpaqueValue&) = default;
  OpaqueValue(OpaqueValue&&) = default;
  OpaqueValue& operator=(const OpaqueValue&) = default;
  OpaqueValue& operator=(OpaqueValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  OpaqueType GetRuntimeType() const { return interface_->GetRuntimeType(); }

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

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return false; }

  OpaqueValue Clone(Allocator<> allocator) const;

  // Returns `true` if this opaque value is an instance of an optional value.
  bool IsOptional() const;

  // Convenience method for use with template metaprogramming. See
  // `IsOptional()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const {
    return IsOptional();
  }

  // Performs a checked cast from an opaque value to an optional value,
  // returning a non-empty optional with either a value or reference to the
  // optional value. Otherwise an empty optional is returned.
  optional_ref<const OptionalValue> AsOptional() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const OptionalValue> AsOptional()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OptionalValue> AsOptional() &&;
  absl::optional<OptionalValue> AsOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `AsOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>,
                       optional_ref<const OptionalValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&&;

  // Performs an unchecked cast from an opaque value to an optional value. In
  // debug builds a best effort is made to crash. If `IsOptional()` would return
  // false, calling this method is undefined behavior.
  const OptionalValue& GetOptional() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const OptionalValue& GetOptional() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OptionalValue GetOptional() &&;
  OptionalValue GetOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `Optional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get()
      const&&;

  void swap(OpaqueValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<OpaqueValue>;

  Shared<const OpaqueValueInterface> interface_;
};

inline void swap(OpaqueValue& lhs, OpaqueValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const OpaqueValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueValue> final {
  static NativeTypeId Id(const OpaqueValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const OpaqueValue& type) {
    return NativeType::SkipDestructor(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<OpaqueValue, T>>,
                               std::is_base_of<OpaqueValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<OpaqueValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<OpaqueValue>::SkipDestructor(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
