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

// `MapValue` represents values of the primitive `map` type. `MapValueView`
// is a non-owning view of `MapValue`. `MapValueInterface` is the abstract
// base class of implementations. `MapValue` and `MapValueView` act as smart
// pointers to `MapValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/value_kind.h"
#include "common/values/legacy_map_value.h"  // IWYU pragma: export
#include "common/values/map_value_interface.h"  // IWYU pragma: export
#include "common/values/parsed_json_map_value.h"
#include "common/values/parsed_map_field_value.h"
#include "common/values/parsed_map_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class MapValueInterface;
class MapValue;
class Value;
class ValueManager;
class TypeManager;

absl::Status CheckMapKey(const Value& key);

class MapValue final {
 public:
  using interface_type = MapValueInterface;

  static constexpr ValueKind kKind = MapValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(const T& value)
      : variant_(
            absl::in_place_type<common_internal::BaseMapValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  // Move constructor for alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(T&& value)
      : variant_(
            absl::in_place_type<common_internal::BaseMapValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(value)) {}

  MapValue() = default;
  MapValue(const MapValue&) = default;
  MapValue(MapValue&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(const ParsedMapFieldValue& other)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(ParsedMapFieldValue&& other)
      : variant_(absl::in_place_type<ParsedMapFieldValue>, std::move(other)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(const ParsedJsonMapValue& other)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(ParsedJsonMapValue&& other)
      : variant_(absl::in_place_type<ParsedJsonMapValue>, std::move(other)) {}

  MapValue& operator=(const MapValue& other) {
    ABSL_DCHECK(this != std::addressof(other))
        << "MapValue should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  MapValue& operator=(MapValue&& other) noexcept {
    ABSL_DCHECK(this != std::addressof(other))
        << "MapValue should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<ParsedMapValue>();
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  void swap(MapValue& other) noexcept { variant_.swap(other.variant_); }

  absl::StatusOr<bool> IsEmpty() const;

  absl::StatusOr<size_t> Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager,
                            const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(ValueManager& value_manager, const Value& key,
                            Value& result) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager,
                            const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(ValueManager& value_manager, ListValue& result) const;
  absl::StatusOr<ListValue> ListKeys(ValueManager& value_manager) const;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  // Returns `true` if this value is an instance of a parsed map value.
  bool IsParsed() const {
    return absl::holds_alternative<ParsedMapValue>(variant_);
  }

  // Convenience method for use with template metaprogramming. See
  // `IsParsed()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, bool> Is() const {
    return IsParsed();
  }

  // Performs a checked cast from a value to a parsed map value,
  // returning a non-empty optional with either a value or reference to the
  // parsed map value. Otherwise an empty optional is returned.
  optional_ref<const ParsedMapValue> AsParsed() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsParsed();
  }
  optional_ref<const ParsedMapValue> AsParsed()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<ParsedMapValue> AsParsed() &&;
  absl::optional<ParsedMapValue> AsParsed() const&& {
    return common_internal::AsOptional(AsParsed());
  }

  // Convenience method for use with template metaprogramming. See
  // `AsParsed()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                       optional_ref<const ParsedMapValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   optional_ref<const ParsedMapValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   absl::optional<ParsedMapValue>>
  As() && {
    return std::move(*this).AsParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>,
                   absl::optional<ParsedMapValue>>
  As() const&& {
    return std::move(*this).AsParsed();
  }

  // Performs an unchecked cast from a value to a parsed map value. In
  // debug builds a best effort is made to crash. If `IsParsed()` would
  // return false, calling this method is undefined behavior.
  const ParsedMapValue& GetParsed() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetParsed();
  }
  const ParsedMapValue& GetParsed() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  ParsedMapValue GetParsed() &&;
  ParsedMapValue GetParsed() const&& { return GetParsed(); }

  // Convenience method for use with template metaprogramming. See
  // `GetParsed()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMapValue, T>, const ParsedMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, const ParsedMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, ParsedMapValue> Get() && {
    return std::move(*this).GetParsed();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMapValue, T>, ParsedMapValue> Get()
      const&& {
    return std::move(*this).GetParsed();
  }

 private:
  friend class Value;
  friend struct NativeTypeTraits<MapValue>;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  // Unlike many of the other derived values, `MapValue` is itself a composed
  // type. This is to avoid making `MapValue` too big and by extension
  // `Value` too big. Instead we store the derived `MapValue` values in
  // `Value` and not `MapValue` itself.
  common_internal::MapValueVariant variant_;
};

inline void swap(MapValue& lhs, MapValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const MapValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<MapValue> final {
  static NativeTypeId Id(const MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          return NativeTypeId::Of(alternative);
        },
        value.variant_);
  }

  static bool SkipDestructor(const MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> bool {
          return NativeType::SkipDestructor(alternative);
        },
        value.variant_);
  }
};

class MapValueBuilder {
 public:
  virtual ~MapValueBuilder() = default;

  virtual absl::Status Put(Value key, Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual MapValue Build() && = 0;
};

using MapValueBuilderPtr = std::unique_ptr<MapValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
