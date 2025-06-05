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

// `OptionalValue` represents values of the `optional_type` type.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_interface.h"
#include "common/values/opaque_value.h"
#include "internal/casts.h"

namespace cel {

class Value;
class ValueManager;
class OptionalValueInterface;
class OptionalValue;

class OptionalValueInterface : public OpaqueValueInterface {
 public:
  using alternative_type = OptionalValue;

  OpaqueType GetRuntimeType() const final { return OptionalType(); }

  absl::string_view GetTypeName() const final { return "optional_type"; }

  std::string DebugString() const final;

  virtual bool HasValue() const = 0;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     cel::Value& result) const override;

  virtual void Value(cel::Value& scratch) const = 0;

  cel::Value Value() const;

 private:
  NativeTypeId GetNativeTypeId() const noexcept final {
    return NativeTypeId::For<OptionalValueInterface>();
  }
};

class OptionalValue final : public OpaqueValue {
 public:
  using interface_type = OptionalValueInterface;

  static OptionalValue None();

  static OptionalValue Of(MemoryManagerRef memory_manager, cel::Value value);

  // Used by SubsumptionTraits to downcast OpaqueType rvalue references.
  explicit OptionalValue(OpaqueValue&& value) noexcept
      : OpaqueValue(std::move(value)) {}

  OptionalValue() : OptionalValue(None()) {}

  OptionalValue(const OptionalValue&) = default;
  OptionalValue(OptionalValue&&) = default;
  OptionalValue& operator=(const OptionalValue&) = default;
  OptionalValue& operator=(OptionalValue&&) = default;

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            OptionalValueInterface, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValue(Shared<T> interface) : OpaqueValue(std::move(interface)) {}

  OptionalType GetRuntimeType() const {
    return (*this)->GetRuntimeType().GetOptional();
  }

  bool HasValue() const { return (*this)->HasValue(); }

  void Value(cel::Value& result) const;

  cel::Value Value() const;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const OptionalValueInterface&>(
        OpaqueValue::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const OptionalValueInterface*>(
        OpaqueValue::operator->());
  }

  bool IsOptional() const = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const = delete;
  optional_ref<const OptionalValue> AsOptional() & = delete;
  optional_ref<const OptionalValue> AsOptional() const& = delete;
  absl::optional<OptionalValue> AsOptional() && = delete;
  absl::optional<OptionalValue> AsOptional() const&& = delete;
  const OptionalValue& GetOptional() & = delete;
  const OptionalValue& GetOptional() const& = delete;
  OptionalValue GetOptional() && = delete;
  OptionalValue GetOptional() const&& = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() & = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() && = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&& = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  Get() & = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  Get() const& = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  Get() && = delete;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  Get() const&& = delete;
};

inline optional_ref<const OptionalValue> OpaqueValue::AsOptional() &
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return std::as_const(*this).AsOptional();
}

inline absl::optional<OptionalValue> OpaqueValue::AsOptional() const&& {
  return common_internal::AsOptional(AsOptional());
}

template <typename T>
    inline std::enable_if_t<std::is_same_v<OptionalValue, T>,
                            optional_ref<const OptionalValue>>
    OpaqueValue::As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return AsOptional();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<OptionalValue, T>,
                        optional_ref<const OptionalValue>>
OpaqueValue::As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return AsOptional();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<OptionalValue, T>,
                        absl::optional<OptionalValue>>
OpaqueValue::As() && {
  return std::move(*this).AsOptional();
}

template <typename T>
inline std::enable_if_t<std::is_same_v<OptionalValue, T>,
                        absl::optional<OptionalValue>>
OpaqueValue::As() const&& {
  return std::move(*this).AsOptional();
}

inline const OptionalValue& OpaqueValue::GetOptional() &
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return std::as_const(*this).GetOptional();
}

inline OptionalValue OpaqueValue::GetOptional() const&& {
  return GetOptional();
}

template <typename T>
    std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
    OpaqueValue::Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return GetOptional();
}

template <typename T>
std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
OpaqueValue::Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return GetOptional();
}

template <typename T>
std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue>
OpaqueValue::Get() && {
  return std::move(*this).GetOptional();
}

template <typename T>
std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue>
OpaqueValue::Get() const&& {
  return std::move(*this).GetOptional();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
