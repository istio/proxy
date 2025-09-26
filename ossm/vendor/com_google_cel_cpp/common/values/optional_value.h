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

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/types/optional.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/values/opaque_value.h"
#include "google/protobuf/arena.h"

namespace cel {

class Value;
class OptionalValue;

namespace common_internal {
OptionalValue MakeOptionalValue(
    const OpaqueValueDispatcher* absl_nonnull dispatcher,
    OpaqueValueContent content);
}

class OptionalValue final : public OpaqueValue {
 public:
  static OptionalValue None();

  static OptionalValue Of(cel::Value value, google::protobuf::Arena* absl_nonnull arena);

  OptionalValue() : OptionalValue(None()) {}
  OptionalValue(const OptionalValue&) = default;
  OptionalValue(OptionalValue&&) = default;
  OptionalValue& operator=(const OptionalValue&) = default;
  OptionalValue& operator=(OptionalValue&&) = default;

  OptionalType GetRuntimeType() const {
    return OpaqueValue::GetRuntimeType().GetOptional();
  }

  bool HasValue() const;

  void Value(cel::Value* absl_nonnull result) const;

  cel::Value Value() const;

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

 private:
  friend OptionalValue common_internal::MakeOptionalValue(
      const OpaqueValueDispatcher* absl_nonnull dispatcher,
      OpaqueValueContent content);

  OptionalValue(const OpaqueValueDispatcher* absl_nonnull dispatcher,
                OpaqueValueContent content)
      : OpaqueValue(dispatcher, content) {}

  using OpaqueValue::content;
  using OpaqueValue::dispatcher;
  using OpaqueValue::interface;
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

namespace common_internal {

inline OptionalValue MakeOptionalValue(
    const OpaqueValueDispatcher* absl_nonnull dispatcher,
    OpaqueValueContent content) {
  return OptionalValue(dispatcher, content);
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
