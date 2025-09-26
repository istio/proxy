// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_VARIANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_VARIANT_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"
#include "common/values/custom_struct_value.h"
#include "common/values/legacy_struct_value.h"
#include "common/values/parsed_message_value.h"

namespace cel::common_internal {

enum class StructValueIndex : uint16_t {
  kParsedMessage = 0,
  kCustom,
  kLegacy,
};

template <typename T>
struct StructValueAlternative;

template <>
struct StructValueAlternative<CustomStructValue> {
  static constexpr StructValueIndex kIndex = StructValueIndex::kCustom;
};

template <>
struct StructValueAlternative<ParsedMessageValue> {
  static constexpr StructValueIndex kIndex = StructValueIndex::kParsedMessage;
};

template <>
struct StructValueAlternative<LegacyStructValue> {
  static constexpr StructValueIndex kIndex = StructValueIndex::kLegacy;
};

template <typename T, typename = void>
struct IsStructValueAlternative : std::false_type {};

template <typename T>
struct IsStructValueAlternative<
    T, std::void_t<decltype(StructValueAlternative<T>{})>> : std::true_type {};

template <typename T>
inline constexpr bool IsStructValueAlternativeV =
    IsStructValueAlternative<T>::value;

inline constexpr size_t kStructValueVariantAlign = 8;
inline constexpr size_t kStructValueVariantSize = 24;

// StructValueVariant is a subset of alternatives from the main ValueVariant
// that is only structs. It is not stored directly in ValueVariant.
class alignas(kStructValueVariantAlign) StructValueVariant final {
 public:
  StructValueVariant()
      : StructValueVariant(absl::in_place_type<ParsedMessageValue>) {}

  StructValueVariant(const StructValueVariant&) = default;
  StructValueVariant(StructValueVariant&&) = default;
  StructValueVariant& operator=(const StructValueVariant&) = default;
  StructValueVariant& operator=(StructValueVariant&&) = default;

  template <typename T, typename... Args>
  explicit StructValueVariant(absl::in_place_type_t<T>, Args&&... args)
      : index_(StructValueAlternative<T>::kIndex) {
    static_assert(alignof(T) <= kStructValueVariantAlign);
    static_assert(sizeof(T) <= kStructValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    ::new (static_cast<void*>(&raw_[0])) T(std::forward<Args>(args)...);
  }

  template <typename T, typename = std::enable_if_t<
                            IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  explicit StructValueVariant(T&& value)
      : StructValueVariant(absl::in_place_type<absl::remove_cvref_t<T>>,
                           std::forward<T>(value)) {}

  template <typename T>
  void Assign(T&& value) {
    using U = absl::remove_cvref_t<T>;

    static_assert(alignof(U) <= kStructValueVariantAlign);
    static_assert(sizeof(U) <= kStructValueVariantSize);
    static_assert(std::is_trivially_copyable_v<U>);

    index_ = StructValueAlternative<U>::kIndex;
    ::new (static_cast<void*>(&raw_[0])) U(std::forward<T>(value));
  }

  template <typename T>
  bool Is() const {
    return index_ == StructValueAlternative<T>::kIndex;
  }

  template <typename T>
      T& Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Is<T>());

    return *At<T>();
  }

  template <typename T>
  const T& Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Is<T>());

    return *At<T>();
  }

  template <typename T>
      T&& Get() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Is<T>());

    return std::move(*At<T>());
  }

  template <typename T>
  const T&& Get() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(Is<T>());

    return std::move(*At<T>());
  }

  template <typename T>
  T* absl_nullable As() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (Is<T>()) {
      return At<T>();
    }
    return nullptr;
  }

  template <typename T>
  const T* absl_nullable As() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (Is<T>()) {
      return At<T>();
    }
    return nullptr;
  }

  template <typename Visitor>
  decltype(auto) Visit(Visitor&& visitor) const {
    switch (index_) {
      case StructValueIndex::kCustom:
        return std::forward<Visitor>(visitor)(Get<CustomStructValue>());
      case StructValueIndex::kParsedMessage:
        return std::forward<Visitor>(visitor)(Get<ParsedMessageValue>());
      case StructValueIndex::kLegacy:
        return std::forward<Visitor>(visitor)(Get<LegacyStructValue>());
    }
  }

  friend void swap(StructValueVariant& lhs, StructValueVariant& rhs) noexcept {
    using std::swap;
    swap(lhs.index_, rhs.index_);
    swap(lhs.raw_, rhs.raw_);
  }

 private:
  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE T* absl_nonnull At()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kStructValueVariantAlign);
    static_assert(sizeof(T) <= kStructValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<T*>(&raw_[0]));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE const T* absl_nonnull At() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kStructValueVariantAlign);
    static_assert(sizeof(T) <= kStructValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<const T*>(&raw_[0]));
  }

  StructValueIndex index_ = StructValueIndex::kCustom;
  alignas(8) std::byte raw_[kStructValueVariantSize];
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_VARIANT_H_
