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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_VARIANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_VARIANT_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"
#include "common/values/custom_list_value.h"
#include "common/values/legacy_list_value.h"
#include "common/values/parsed_json_list_value.h"
#include "common/values/parsed_repeated_field_value.h"

namespace cel::common_internal {

enum class ListValueIndex : uint16_t {
  kCustom = 0,
  kParsedField,
  kParsedJson,
  kLegacy,
};

template <typename T>
struct ListValueAlternative;

template <>
struct ListValueAlternative<CustomListValue> {
  static constexpr ListValueIndex kIndex = ListValueIndex::kCustom;
};

template <>
struct ListValueAlternative<ParsedRepeatedFieldValue> {
  static constexpr ListValueIndex kIndex = ListValueIndex::kParsedField;
};

template <>
struct ListValueAlternative<ParsedJsonListValue> {
  static constexpr ListValueIndex kIndex = ListValueIndex::kParsedJson;
};

template <>
struct ListValueAlternative<LegacyListValue> {
  static constexpr ListValueIndex kIndex = ListValueIndex::kLegacy;
};

template <typename T, typename = void>
struct IsListValueAlternative : std::false_type {};

template <typename T>
struct IsListValueAlternative<T,
                              std::void_t<decltype(ListValueAlternative<T>{})>>
    : std::true_type {};

template <typename T>
inline constexpr bool IsListValueAlternativeV =
    IsListValueAlternative<T>::value;

inline constexpr size_t kListValueVariantAlign = 8;
inline constexpr size_t kListValueVariantSize = 24;

// ListValueVariant is a subset of alternatives from the main ValueVariant that
// is only lists. It is not stored directly in ValueVariant.
class alignas(kListValueVariantAlign) ListValueVariant final {
 public:
  ListValueVariant() : ListValueVariant(absl::in_place_type<CustomListValue>) {}

  ListValueVariant(const ListValueVariant&) = default;
  ListValueVariant(ListValueVariant&&) = default;
  ListValueVariant& operator=(const ListValueVariant&) = default;
  ListValueVariant& operator=(ListValueVariant&&) = default;

  template <typename T, typename... Args>
  explicit ListValueVariant(absl::in_place_type_t<T>, Args&&... args)
      : index_(ListValueAlternative<T>::kIndex) {
    static_assert(alignof(T) <= kListValueVariantAlign);
    static_assert(sizeof(T) <= kListValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    ::new (static_cast<void*>(&raw_[0])) T(std::forward<Args>(args)...);
  }

  template <typename T, typename = std::enable_if_t<
                            IsListValueAlternativeV<absl::remove_cvref_t<T>>>>
  explicit ListValueVariant(T&& value)
      : ListValueVariant(absl::in_place_type<absl::remove_cvref_t<T>>,
                         std::forward<T>(value)) {}

  template <typename T>
  void Assign(T&& value) {
    using U = absl::remove_cvref_t<T>;

    static_assert(alignof(U) <= kListValueVariantAlign);
    static_assert(sizeof(U) <= kListValueVariantSize);
    static_assert(std::is_trivially_copyable_v<U>);

    index_ = ListValueAlternative<U>::kIndex;
    ::new (static_cast<void*>(&raw_[0])) U(std::forward<T>(value));
  }

  template <typename T>
  bool Is() const {
    return index_ == ListValueAlternative<T>::kIndex;
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
      case ListValueIndex::kCustom:
        return std::forward<Visitor>(visitor)(Get<CustomListValue>());
      case ListValueIndex::kParsedField:
        return std::forward<Visitor>(visitor)(Get<ParsedRepeatedFieldValue>());
      case ListValueIndex::kParsedJson:
        return std::forward<Visitor>(visitor)(Get<ParsedJsonListValue>());
      case ListValueIndex::kLegacy:
        return std::forward<Visitor>(visitor)(Get<LegacyListValue>());
    }
  }

  friend void swap(ListValueVariant& lhs, ListValueVariant& rhs) noexcept {
    using std::swap;
    swap(lhs.index_, rhs.index_);
    swap(lhs.raw_, rhs.raw_);
  }

 private:
  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE T* absl_nonnull At()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kListValueVariantAlign);
    static_assert(sizeof(T) <= kListValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<T*>(&raw_[0]));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE const T* absl_nonnull At() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kListValueVariantAlign);
    static_assert(sizeof(T) <= kListValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<const T*>(&raw_[0]));
  }

  ListValueIndex index_ = ListValueIndex::kCustom;
  alignas(8) std::byte raw_[kListValueVariantSize];
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_VARIANT_H_
