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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_VARIANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_VARIANT_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"
#include "common/values/custom_map_value.h"
#include "common/values/legacy_map_value.h"
#include "common/values/parsed_json_map_value.h"
#include "common/values/parsed_map_field_value.h"

namespace cel::common_internal {

enum class MapValueIndex : uint16_t {
  kCustom = 0,
  kParsedField,
  kParsedJson,
  kLegacy,
};

template <typename T>
struct MapValueAlternative;

template <>
struct MapValueAlternative<CustomMapValue> {
  static constexpr MapValueIndex kIndex = MapValueIndex::kCustom;
};

template <>
struct MapValueAlternative<ParsedMapFieldValue> {
  static constexpr MapValueIndex kIndex = MapValueIndex::kParsedField;
};

template <>
struct MapValueAlternative<ParsedJsonMapValue> {
  static constexpr MapValueIndex kIndex = MapValueIndex::kParsedJson;
};

template <>
struct MapValueAlternative<LegacyMapValue> {
  static constexpr MapValueIndex kIndex = MapValueIndex::kLegacy;
};

template <typename T, typename = void>
struct IsMapValueAlternative : std::false_type {};

template <typename T>
struct IsMapValueAlternative<T, std::void_t<decltype(MapValueAlternative<T>{})>>
    : std::true_type {};

template <typename T>
inline constexpr bool IsMapValueAlternativeV = IsMapValueAlternative<T>::value;

inline constexpr size_t kMapValueVariantAlign = 8;
inline constexpr size_t kMapValueVariantSize = 24;

// MapValueVariant is a subset of alternatives from the main ValueVariant that
// is only maps. It is not stored directly in ValueVariant.
class alignas(kMapValueVariantAlign) MapValueVariant final {
 public:
  MapValueVariant() : MapValueVariant(absl::in_place_type<CustomMapValue>) {}

  MapValueVariant(const MapValueVariant&) = default;
  MapValueVariant(MapValueVariant&&) = default;
  MapValueVariant& operator=(const MapValueVariant&) = default;
  MapValueVariant& operator=(MapValueVariant&&) = default;

  template <typename T, typename... Args>
  explicit MapValueVariant(absl::in_place_type_t<T>, Args&&... args)
      : index_(MapValueAlternative<T>::kIndex) {
    static_assert(alignof(T) <= kMapValueVariantAlign);
    static_assert(sizeof(T) <= kMapValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    ::new (static_cast<void*>(&raw_[0])) T(std::forward<Args>(args)...);
  }

  template <typename T, typename = std::enable_if_t<
                            IsMapValueAlternativeV<absl::remove_cvref_t<T>>>>
  explicit MapValueVariant(T&& value)
      : MapValueVariant(absl::in_place_type<absl::remove_cvref_t<T>>,
                        std::forward<T>(value)) {}

  template <typename T>
  void Assign(T&& value) {
    using U = absl::remove_cvref_t<T>;

    static_assert(alignof(U) <= kMapValueVariantAlign);
    static_assert(sizeof(U) <= kMapValueVariantSize);
    static_assert(std::is_trivially_copyable_v<U>);

    index_ = MapValueAlternative<U>::kIndex;
    ::new (static_cast<void*>(&raw_[0])) U(std::forward<T>(value));
  }

  template <typename T>
  bool Is() const {
    return index_ == MapValueAlternative<T>::kIndex;
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
      case MapValueIndex::kCustom:
        return std::forward<Visitor>(visitor)(Get<CustomMapValue>());
      case MapValueIndex::kParsedField:
        return std::forward<Visitor>(visitor)(Get<ParsedMapFieldValue>());
      case MapValueIndex::kParsedJson:
        return std::forward<Visitor>(visitor)(Get<ParsedJsonMapValue>());
      case MapValueIndex::kLegacy:
        return std::forward<Visitor>(visitor)(Get<LegacyMapValue>());
    }
  }

  friend void swap(MapValueVariant& lhs, MapValueVariant& rhs) noexcept {
    using std::swap;
    swap(lhs.index_, rhs.index_);
    swap(lhs.raw_, rhs.raw_);
  }

 private:
  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE T* absl_nonnull At()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kMapValueVariantAlign);
    static_assert(sizeof(T) <= kMapValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<T*>(&raw_[0]));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE const T* absl_nonnull At() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kMapValueVariantAlign);
    static_assert(sizeof(T) <= kMapValueVariantSize);
    static_assert(std::is_trivially_copyable_v<T>);

    return std::launder(reinterpret_cast<const T*>(&raw_[0]));
  }

  MapValueIndex index_ = MapValueIndex::kCustom;
  alignas(8) std::byte raw_[kMapValueVariantSize];
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_VARIANT_H_
