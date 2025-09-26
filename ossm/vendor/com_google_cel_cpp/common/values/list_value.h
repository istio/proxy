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

// `ListValue` represents values of the primitive `list` type.
// `ListValueInterface` is the abstract base class of implementations.
// `ListValue` acts as a smart pointer to `ListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_

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
#include "common/values/custom_list_value.h"
#include "common/values/legacy_list_value.h"
#include "common/values/list_value_variant.h"
#include "common/values/parsed_json_list_value.h"
#include "common/values/parsed_repeated_field_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class ListValueInterface;
class ListValue;
class Value;

class ListValue final : private common_internal::ListValueMixin<ListValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;

  // Move constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsListValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(T&& value)
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(value)) {}

  ListValue() = default;
  ListValue(const ListValue&) = default;
  ListValue(ListValue&&) = default;
  ListValue& operator=(const ListValue&) = default;
  ListValue& operator=(ListValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return "list"; }

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
  // `google.protobuf.ListValue`.
  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using ListValueMixin::Equal;

  bool IsZeroValue() const;

  absl::StatusOr<bool> IsEmpty() const;

  absl::StatusOr<size_t> Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const;
  using ListValueMixin::Get;

  using ForEachCallback = typename CustomListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename CustomListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;
  using ListValueMixin::ForEach;

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const;

  absl::Status Contains(
      const Value& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const;
  using ListValueMixin::Contains;

  // Returns `true` if this value is an instance of a custom list value.
  bool IsCustom() const { return variant_.Is<CustomListValue>(); }

  // Convenience method for use with template metaprogramming. See
  // `IsParsed()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, bool> Is() const {
    return IsCustom();
  }

  // Performs a checked cast from a value to a custom list value,
  // returning a non-empty optional with either a value or reference to the
  // custom list value. Otherwise an empty optional is returned.
  optional_ref<const CustomListValue> AsCustom() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).AsCustom();
  }
  optional_ref<const CustomListValue> AsCustom()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<CustomListValue> AsCustom() &&;
  absl::optional<CustomListValue> AsCustom() const&& {
    return common_internal::AsOptional(AsCustom());
  }

  // Convenience method for use with template metaprogramming. See
  // `AsCustom()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomListValue, T>,
                       optional_ref<const CustomListValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   optional_ref<const CustomListValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   absl::optional<CustomListValue>>
  As() && {
    return std::move(*this).AsCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>,
                   absl::optional<CustomListValue>>
  As() const&& {
    return std::move(*this).AsCustom();
  }

  // Performs an unchecked cast from a value to a custom list value. In
  // debug builds a best effort is made to crash. If `IsCustom()` would
  // return false, calling this method is undefined behavior.
  const CustomListValue& GetCustom() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::as_const(*this).GetCustom();
  }
  const CustomListValue& GetCustom() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  CustomListValue GetCustom() &&;
  CustomListValue GetCustom() const&& { return GetCustom(); }

  // Convenience method for use with template metaprogramming. See
  // `GetCustom()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<CustomListValue, T>,
                       const CustomListValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, const CustomListValue&>
  Get() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, CustomListValue>
  Get() && {
    return std::move(*this).GetCustom();
  }
  template <typename T>
  std::enable_if_t<std::is_same_v<CustomListValue, T>, CustomListValue> Get()
      const&& {
    return std::move(*this).GetCustom();
  }

  friend void swap(ListValue& lhs, ListValue& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

 private:
  friend class Value;
  friend class common_internal::ValueMixin<ListValue>;
  friend class common_internal::ListValueMixin<ListValue>;

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  // Unlike many of the other derived values, `ListValue` is itself a composed
  // type. This is to avoid making `ListValue` too big and by extension
  // `Value` too big. Instead we store the derived `ListValue` values in
  // `Value` and not `ListValue` itself.
  common_internal::ListValueVariant variant_;
};

inline std::ostream& operator<<(std::ostream& out, const ListValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ListValue> final {
  static NativeTypeId Id(const ListValue& value) { return value.GetTypeId(); }
};

class ListValueBuilder {
 public:
  virtual ~ListValueBuilder() = default;

  virtual absl::Status Add(Value value) = 0;

  virtual void UnsafeAdd(Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual ListValue Build() && = 0;
};

using ListValueBuilderPtr = std::unique_ptr<ListValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
