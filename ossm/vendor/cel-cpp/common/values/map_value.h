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
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/value_kind.h"
#include "common/values/custom_map_value.h"
#include "common/values/legacy_map_value.h"
#include "common/values/map_value_variant.h"
#include "common/values/parsed_json_map_value.h"
#include "common/values/parsed_map_field_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class MapValueInterface;
class MapValue;
class Value;

absl::Status CheckMapKey(const Value& key);

class MapValue final : private common_internal::MapValueMixin<MapValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;

  // Move constructor for alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(T&& value)
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(value)) {}

  MapValue() = default;
  MapValue(const MapValue&) = default;
  MapValue(MapValue&&) = default;
  MapValue& operator=(const MapValue&) = default;
  MapValue& operator=(MapValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  static absl::string_view GetTypeName() { return "map"; }

  NativeTypeId GetTypeId() const;

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

  // Like ConvertToJson(), except `json` **MUST** be an instance of
  // `google.protobuf.Struct`.
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using MapValueMixin::Equal;

  bool IsZeroValue() const;

  absl::StatusOr<bool> IsEmpty() const;

  absl::StatusOr<size_t> Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using MapValueMixin::Get;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using MapValueMixin::Find;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using MapValueMixin::Has;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const;
  using MapValueMixin::ListKeys;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename CustomMapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  // Returns `true` if this value is an instance of a custom map value.
  bool IsCustom() const { return variant_.Is<CustomMapValue>(); }

  // Convenience method for use with template metaprogramming. See
  // `IsCustom()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, bool> Is() const {
    return IsCustom();
  }

  // Performs a checked cast from a value to a custom map value,
  // returning a non-empty optional with either a value or reference to the
  // custom map value. Otherwise an empty optional is returned.
  optional_ref<const CustomMapValue> AsCustom() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsCustom();
  }
  optional_ref<const CustomMapValue> AsCustom()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<CustomMapValue> AsCustom() &&;
  absl::optional<CustomMapValue> AsCustom() const&& {
    return common_internal::AsOptional(AsCustom());
  }

  // Convenience method for use with template metaprogramming. See
  // `AsCustom()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                       optional_ref<const CustomMapValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   optional_ref<const CustomMapValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   absl::optional<CustomMapValue>>
  As() && {
    return std::move(*this).AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>,
                   absl::optional<CustomMapValue>>
  As() const&& {
    return std::move(*this).AsCustom();
  }

  // Performs an unchecked cast from a value to a custom map value. In
  // debug builds a best effort is made to crash. If `IsCustom()` would
  // return false, calling this method is undefined behavior.
  const CustomMapValue& GetCustom() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetCustom();
  }
  const CustomMapValue& GetCustom() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  CustomMapValue GetCustom() &&;
  CustomMapValue GetCustom() const&& { return GetCustom(); }

  // Convenience method for use with template metaprogramming. See
  // `GetCustom()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomMapValue, T>, const CustomMapValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, const CustomMapValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, CustomMapValue> Get() && {
    return std::move(*this).GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomMapValue, T>, CustomMapValue> Get()
      const&& {
    return std::move(*this).GetCustom();
  }

  friend void swap(MapValue& lhs, MapValue& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

 private:
  friend class Value;
  friend class common_internal::ValueMixin<MapValue>;
  friend class common_internal::MapValueMixin<MapValue>;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  // Unlike many of the other derived values, `MapValue` is itself a composed
  // type. This is to avoid making `MapValue` too big and by extension
  // `Value` too big. Instead we store the derived `MapValue` values in
  // `Value` and not `MapValue` itself.
  common_internal::MapValueVariant variant_;
};

inline std::ostream& operator<<(std::ostream& out, const MapValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<MapValue> final {
  static NativeTypeId Id(const MapValue& value) { return value.GetTypeId(); }
};

class MapValueBuilder {
 public:
  virtual ~MapValueBuilder() = default;

  virtual absl::Status Put(Value key, Value value) = 0;

  virtual void UnsafePut(Value key, Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity [[maybe_unused]]) {}

  virtual MapValue Build() && = 0;
};

using MapValueBuilderPtr = std::unique_ptr<MapValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
