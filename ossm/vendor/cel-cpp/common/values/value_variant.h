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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_VARIANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_VARIANT_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/utility/utility.h"
#include "common/arena.h"
#include "common/value_kind.h"
#include "common/values/bool_value.h"
#include "common/values/bytes_value.h"
#include "common/values/custom_list_value.h"
#include "common/values/custom_map_value.h"
#include "common/values/custom_struct_value.h"
#include "common/values/double_value.h"
#include "common/values/duration_value.h"
#include "common/values/error_value.h"
#include "common/values/int_value.h"
#include "common/values/legacy_list_value.h"
#include "common/values/legacy_map_value.h"
#include "common/values/legacy_struct_value.h"
#include "common/values/list_value.h"
#include "common/values/map_value.h"
#include "common/values/null_value.h"
#include "common/values/opaque_value.h"
#include "common/values/parsed_json_list_value.h"
#include "common/values/parsed_json_map_value.h"
#include "common/values/parsed_map_field_value.h"
#include "common/values/parsed_message_value.h"
#include "common/values/parsed_repeated_field_value.h"
#include "common/values/string_value.h"
#include "common/values/timestamp_value.h"
#include "common/values/type_value.h"
#include "common/values/uint_value.h"
#include "common/values/unknown_value.h"
#include "common/values/values.h"

namespace cel {

class Value;

namespace common_internal {

// Used by ValueVariant to indicate the active alternative.
enum class ValueIndex : uint8_t {
  kNull = 0,
  kBool,
  kInt,
  kUint,
  kDouble,
  kDuration,
  kTimestamp,
  kType,
  kLegacyList,
  kParsedJsonList,
  kParsedRepeatedField,
  kCustomList,
  kLegacyMap,
  kParsedJsonMap,
  kParsedMapField,
  kCustomMap,
  kLegacyStruct,
  kParsedMessage,
  kCustomStruct,
  kOpaque,

  // Keep non-trivial alternatives together to aid in compiling optimizations.
  kBytes,
  kString,
  kError,
  kUnknown,
};

// Used by ValueVariant to indicate pre-computed behaviors.
enum class ValueFlags : uint32_t {
  kNone = 0,
  kNonTrivial = 1,
};

ABSL_ATTRIBUTE_ALWAYS_INLINE inline constexpr ValueFlags operator&(
    ValueFlags lhs, ValueFlags rhs) {
  return static_cast<ValueFlags>(
      static_cast<std::underlying_type_t<ValueFlags>>(lhs) &
      static_cast<std::underlying_type_t<ValueFlags>>(rhs));
}

// Traits specialized by each alternative.
//
// ValueIndex ValueAlternative<T>::kIndex
//
//   Indicates the alternative index corresponding to T.
//
// ValueKind ValueAlternative<T>::kKind
//
//  Indicatates the kind corresponding to T.
//
// bool ValueAlternative<T>::kAlwaysTrivial
//
//  True if T is trivially_copyable, false otherwise.
//
// ValueFlags ValueAlternative<T>::Flags(const T* absl_nonnull )
//
//  Returns the flags for the corresponding instance of T.
template <typename T>
struct ValueAlternative;

template <>
struct ValueAlternative<NullValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kNull;
  static constexpr ValueKind kKind = NullValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const NullValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<BoolValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kBool;
  static constexpr ValueKind kKind = BoolValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const BoolValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<IntValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kInt;
  static constexpr ValueKind kKind = IntValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const IntValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<UintValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kUint;
  static constexpr ValueKind kKind = UintValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const UintValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<DoubleValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kDouble;
  static constexpr ValueKind kKind = DoubleValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const DoubleValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<DurationValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kDuration;
  static constexpr ValueKind kKind = DurationValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const DurationValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<TimestampValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kTimestamp;
  static constexpr ValueKind kKind = TimestampValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const TimestampValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<TypeValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kType;
  static constexpr ValueKind kKind = TypeValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const TypeValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<LegacyListValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kLegacyList;
  static constexpr ValueKind kKind = LegacyListValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const LegacyListValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<ParsedJsonListValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kParsedJsonList;
  static constexpr ValueKind kKind = ParsedJsonListValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const ParsedJsonListValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<ParsedRepeatedFieldValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kParsedRepeatedField;
  static constexpr ValueKind kKind = ParsedRepeatedFieldValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(
      const ParsedRepeatedFieldValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<CustomListValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kCustomList;
  static constexpr ValueKind kKind = CustomListValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const CustomListValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<LegacyMapValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kLegacyMap;
  static constexpr ValueKind kKind = LegacyMapValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const LegacyMapValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<ParsedJsonMapValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kParsedJsonMap;
  static constexpr ValueKind kKind = ParsedJsonMapValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const ParsedJsonMapValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<ParsedMapFieldValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kParsedMapField;
  static constexpr ValueKind kKind = ParsedMapFieldValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const ParsedMapFieldValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<CustomMapValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kCustomMap;
  static constexpr ValueKind kKind = CustomMapValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const CustomMapValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<LegacyStructValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kLegacyStruct;
  static constexpr ValueKind kKind = LegacyStructValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const LegacyStructValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<ParsedMessageValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kParsedMessage;
  static constexpr ValueKind kKind = ParsedMessageValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const ParsedMessageValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<CustomStructValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kCustomStruct;
  static constexpr ValueKind kKind = CustomStructValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const CustomStructValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<OpaqueValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kOpaque;
  static constexpr ValueKind kKind = OpaqueValue::kKind;
  static constexpr bool kAlwaysTrivial = true;

  static constexpr ValueFlags Flags(const OpaqueValue* absl_nonnull) {
    return ValueFlags::kNone;
  }
};

template <>
struct ValueAlternative<BytesValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kBytes;
  static constexpr ValueKind kKind = BytesValue::kKind;
  static constexpr bool kAlwaysTrivial = false;

  static ValueFlags Flags(const BytesValue* absl_nonnull alternative) {
    return ArenaTraits<BytesValue>::trivially_destructible(*alternative)
               ? ValueFlags::kNone
               : ValueFlags::kNonTrivial;
  }
};

template <>
struct ValueAlternative<StringValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kString;
  static constexpr ValueKind kKind = StringValue::kKind;
  static constexpr bool kAlwaysTrivial = false;

  static ValueFlags Flags(const StringValue* absl_nonnull alternative) {
    return ArenaTraits<StringValue>::trivially_destructible(*alternative)
               ? ValueFlags::kNone
               : ValueFlags::kNonTrivial;
  }
};

template <>
struct ValueAlternative<ErrorValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kError;
  static constexpr ValueKind kKind = ErrorValue::kKind;
  static constexpr bool kAlwaysTrivial = false;

  static ValueFlags Flags(const ErrorValue* absl_nonnull alternative) {
    return ArenaTraits<ErrorValue>::trivially_destructible(*alternative)
               ? ValueFlags::kNone
               : ValueFlags::kNonTrivial;
  }
};

template <>
struct ValueAlternative<UnknownValue> {
  static constexpr ValueIndex kIndex = ValueIndex::kUnknown;
  static constexpr ValueKind kKind = UnknownValue::kKind;
  static constexpr bool kAlwaysTrivial = false;

  static constexpr ValueFlags Flags(const UnknownValue* absl_nonnull) {
    return ValueFlags::kNonTrivial;
  }
};

template <typename T, typename = void>
struct IsValueAlternative : std::false_type {};

template <typename T>
struct IsValueAlternative<T, std::void_t<decltype(ValueAlternative<T>{})>>
    : std::true_type {};

template <typename T>
inline constexpr bool IsValueAlternativeV = IsValueAlternative<T>::value;

// Alignment and size of the storage inside ValueVariant, not for ValueVariant
// itself.
inline constexpr size_t kValueVariantAlign = 8;
inline constexpr size_t kValueVariantSize = 24;

// Hand-rolled variant used by cel::Value which exhibits up to a 25% performance
// improvement compared to using std::variant.
//
// The implementation abuses the fact that most alternatives are trivially
// copyable and some are conditionally trivially copyable at runtime. For the
// fast path, we perform raw byte copying. For the slow path, we fallback to a
// non-inlined function. The compiler is typically smart enough to inline the
// fast path and emit efficient instructions for the raw byte copying (usually
// two instructions). It also uses switch for visiting, which most compilers can
// optimize better compared to a function pointer table (which libc++ currently
// uses and Clang currently does not optimize well).
class alignas(kValueVariantAlign) CEL_COMMON_INTERNAL_VALUE_VARIANT_TRIVIAL_ABI
    ValueVariant final {
 public:
  ValueVariant() = default;

  ValueVariant(const ValueVariant& other) noexcept
      : index_(other.index_), kind_(other.kind_), flags_(other.flags_) {
    if ((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone) {
      std::memcpy(raw_, other.raw_, sizeof(raw_));
    } else {
      SlowCopyConstruct(other);
    }
  }

  ValueVariant(ValueVariant&& other) noexcept
      : index_(other.index_), kind_(other.kind_), flags_(other.flags_) {
    if ((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone) {
      std::memcpy(raw_, other.raw_, sizeof(raw_));
    } else {
      SlowMoveConstruct(other);
    }
  }

  ~ValueVariant() {
    if ((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNonTrivial) {
      SlowDestruct();
    }
  }

  ValueVariant& operator=(const ValueVariant& other) noexcept {
    if (this != &other) {
      const bool trivial =
          (flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      const bool other_trivial =
          (other.flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      if (trivial && other_trivial) {
        FastCopyAssign(other);
      } else {
        SlowCopyAssign(other, trivial, other_trivial);
      }
    }
    return *this;
  }

  ValueVariant& operator=(ValueVariant&& other) noexcept {
    if (this != &other) {
      const bool trivial =
          (flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      const bool other_trivial =
          (other.flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      if (trivial && other_trivial) {
        FastMoveAssign(other);
      } else {
        SlowMoveAssign(other, trivial, other_trivial);
      }
    }
    return *this;
  }

  template <typename T, typename... Args>
  explicit ValueVariant(absl::in_place_type_t<T>, Args&&... args)
      : index_(ValueAlternative<T>::kIndex), kind_(ValueAlternative<T>::kKind) {
    static_assert(alignof(T) <= kValueVariantAlign);
    static_assert(sizeof(T) <= kValueVariantSize);

    flags_ = ValueAlternative<T>::Flags(::new (static_cast<void*>(&raw_[0]))
                                            T(std::forward<Args>(args)...));
  }

  template <typename T, typename = std::enable_if_t<
                            IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  explicit ValueVariant(T&& value)
      : ValueVariant(absl::in_place_type<absl::remove_cvref_t<T>>,
                     std::forward<T>(value)) {}

  ValueKind kind() const { return kind_; }

  template <typename T>
  void Assign(T&& value) {
    using U = absl::remove_cvref_t<T>;

    static_assert(alignof(U) <= kValueVariantAlign);
    static_assert(sizeof(U) <= kValueVariantSize);

    if constexpr (ValueAlternative<U>::kAlwaysTrivial) {
      if ((flags_ & ValueFlags::kNonTrivial) != ValueFlags::kNone) {
        SlowDestruct();
      }
      index_ = ValueAlternative<U>::kIndex;
      kind_ = ValueAlternative<U>::kKind;
      flags_ = ValueAlternative<U>::Flags(::new (static_cast<void*>(&raw_[0]))
                                              U(std::forward<T>(value)));
    } else {
      // U is not always trivial. See if the current active alternative is U. If
      // it is, we can just do a simple assignment without having to destruct
      // first. Otherwise fallback to destruct and construct.
      if (index_ == ValueAlternative<U>::kIndex) {
        *At<U>() = std::forward<T>(value);
        flags_ = ValueAlternative<U>::Flags(At<U>());
      } else {
        if ((flags_ & ValueFlags::kNonTrivial) != ValueFlags::kNone) {
          SlowDestruct();
        }
        index_ = ValueAlternative<U>::kIndex;
        kind_ = ValueAlternative<U>::kKind;
        flags_ = ValueAlternative<U>::Flags(::new (static_cast<void*>(&raw_[0]))
                                                U(std::forward<T>(value)));
      }
    }
  }

  template <typename T>
  bool Is() const {
    return index_ == ValueAlternative<T>::kIndex;
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
  ABSL_ATTRIBUTE_ALWAYS_INLINE decltype(auto) Visit(Visitor&& visitor) & {
    return std::as_const(*this).Visit(std::forward<Visitor>(visitor));
  }

  template <typename Visitor>
  decltype(auto) Visit(Visitor&& visitor) const& {
    switch (index_) {
      case ValueIndex::kNull:
        return std::forward<Visitor>(visitor)(Get<NullValue>());
      case ValueIndex::kBool:
        return std::forward<Visitor>(visitor)(Get<BoolValue>());
      case ValueIndex::kInt:
        return std::forward<Visitor>(visitor)(Get<IntValue>());
      case ValueIndex::kUint:
        return std::forward<Visitor>(visitor)(Get<UintValue>());
      case ValueIndex::kDouble:
        return std::forward<Visitor>(visitor)(Get<DoubleValue>());
      case ValueIndex::kDuration:
        return std::forward<Visitor>(visitor)(Get<DurationValue>());
      case ValueIndex::kTimestamp:
        return std::forward<Visitor>(visitor)(Get<TimestampValue>());
      case ValueIndex::kType:
        return std::forward<Visitor>(visitor)(Get<TypeValue>());
      case ValueIndex::kLegacyList:
        return std::forward<Visitor>(visitor)(Get<LegacyListValue>());
      case ValueIndex::kParsedJsonList:
        return std::forward<Visitor>(visitor)(Get<ParsedJsonListValue>());
      case ValueIndex::kParsedRepeatedField:
        return std::forward<Visitor>(visitor)(Get<ParsedRepeatedFieldValue>());
      case ValueIndex::kCustomList:
        return std::forward<Visitor>(visitor)(Get<CustomListValue>());
      case ValueIndex::kLegacyMap:
        return std::forward<Visitor>(visitor)(Get<LegacyMapValue>());
      case ValueIndex::kParsedJsonMap:
        return std::forward<Visitor>(visitor)(Get<ParsedJsonMapValue>());
      case ValueIndex::kParsedMapField:
        return std::forward<Visitor>(visitor)(Get<ParsedMapFieldValue>());
      case ValueIndex::kCustomMap:
        return std::forward<Visitor>(visitor)(Get<CustomMapValue>());
      case ValueIndex::kLegacyStruct:
        return std::forward<Visitor>(visitor)(Get<LegacyStructValue>());
      case ValueIndex::kParsedMessage:
        return std::forward<Visitor>(visitor)(Get<ParsedMessageValue>());
      case ValueIndex::kCustomStruct:
        return std::forward<Visitor>(visitor)(Get<CustomStructValue>());
      case ValueIndex::kOpaque:
        return std::forward<Visitor>(visitor)(Get<OpaqueValue>());
      case ValueIndex::kBytes:
        return std::forward<Visitor>(visitor)(Get<BytesValue>());
      case ValueIndex::kString:
        return std::forward<Visitor>(visitor)(Get<StringValue>());
      case ValueIndex::kError:
        return std::forward<Visitor>(visitor)(Get<ErrorValue>());
      case ValueIndex::kUnknown:
        return std::forward<Visitor>(visitor)(Get<UnknownValue>());
    }
  }

  template <typename Visitor>
  decltype(auto) Visit(Visitor&& visitor) && {
    switch (index_) {
      case ValueIndex::kNull:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<NullValue>());
      case ValueIndex::kBool:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<BoolValue>());
      case ValueIndex::kInt:
        return std::forward<Visitor>(visitor)(std::move(*this).Get<IntValue>());
      case ValueIndex::kUint:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<UintValue>());
      case ValueIndex::kDouble:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<DoubleValue>());
      case ValueIndex::kDuration:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<DurationValue>());
      case ValueIndex::kTimestamp:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<TimestampValue>());
      case ValueIndex::kType:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<TypeValue>());
      case ValueIndex::kLegacyList:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<LegacyListValue>());
      case ValueIndex::kParsedJsonList:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ParsedJsonListValue>());
      case ValueIndex::kParsedRepeatedField:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ParsedRepeatedFieldValue>());
      case ValueIndex::kCustomList:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<CustomListValue>());
      case ValueIndex::kLegacyMap:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<LegacyMapValue>());
      case ValueIndex::kParsedJsonMap:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ParsedJsonMapValue>());
      case ValueIndex::kParsedMapField:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ParsedMapFieldValue>());
      case ValueIndex::kCustomMap:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<CustomMapValue>());
      case ValueIndex::kLegacyStruct:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<LegacyStructValue>());
      case ValueIndex::kParsedMessage:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ParsedMessageValue>());
      case ValueIndex::kCustomStruct:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<CustomStructValue>());
      case ValueIndex::kOpaque:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<OpaqueValue>());
      case ValueIndex::kBytes:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<BytesValue>());
      case ValueIndex::kString:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<StringValue>());
      case ValueIndex::kError:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<ErrorValue>());
      case ValueIndex::kUnknown:
        return std::forward<Visitor>(visitor)(
            std::move(*this).Get<UnknownValue>());
    }
  }

  template <typename Visitor>
  ABSL_ATTRIBUTE_ALWAYS_INLINE decltype(auto) Visit(Visitor&& visitor) const&& {
    return Visit(std::forward<Visitor>(visitor));
  }

  friend void swap(ValueVariant& lhs, ValueVariant& rhs) noexcept {
    if (&lhs != &rhs) {
      const bool lhs_trivial =
          (lhs.flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      const bool rhs_trivial =
          (rhs.flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNone;
      if (lhs_trivial && rhs_trivial) {
// We validated the instances can be copied byte-wise at runtime, but compilers
// warn since this is not safe in the general case.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#elif defined(__clang__) && __clang_major__ >= 20
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnontrivial-memcall"
#endif
        alignas(ValueVariant) std::byte tmp[sizeof(ValueVariant)];
        // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
        std::memcpy(tmp, std::addressof(lhs), sizeof(ValueVariant));
        // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
        std::memcpy(std::addressof(lhs), std::addressof(rhs),
                    sizeof(ValueVariant));
        // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
        std::memcpy(std::addressof(rhs), tmp, sizeof(ValueVariant));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__) && __clang_major__ >= 20
#pragma clang diagnostic pop
#endif
      } else {
        SlowSwap(lhs, rhs, lhs_trivial, rhs_trivial);
      }
    }
  }

 private:
  friend struct cel::ArenaTraits<ValueVariant>;

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE T* absl_nonnull At()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kValueVariantAlign);
    static_assert(sizeof(T) <= kValueVariantSize);

    return std::launder(reinterpret_cast<T*>(&raw_[0]));
  }

  template <typename T>
  ABSL_ATTRIBUTE_ALWAYS_INLINE const T* absl_nonnull At() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    static_assert(alignof(T) <= kValueVariantAlign);
    static_assert(sizeof(T) <= kValueVariantSize);

    return std::launder(reinterpret_cast<const T*>(&raw_[0]));
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE void FastCopyAssign(
      const ValueVariant& other) noexcept {
    index_ = other.index_;
    kind_ = other.kind_;
    flags_ = other.flags_;
    std::memcpy(raw_, other.raw_, sizeof(raw_));
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE void FastMoveAssign(
      ValueVariant& other) noexcept {
    FastCopyAssign(other);
  }

  void SlowCopyConstruct(const ValueVariant& other) noexcept;

  void SlowMoveConstruct(ValueVariant& other) noexcept;

  void SlowDestruct() noexcept;

  void SlowCopyAssign(const ValueVariant& other, bool trivial,
                      bool other_trivial) noexcept;

  void SlowMoveAssign(ValueVariant& other, bool ntrivial,
                      bool other_trivial) noexcept;

  static void SlowSwap(ValueVariant& lhs, ValueVariant& rhs, bool lhs_trivial,
                       bool rhs_trivial) noexcept;

  ValueIndex index_ = ValueIndex::kNull;
  ValueKind kind_ = ValueKind::kNull;
  ValueFlags flags_ = ValueFlags::kNone;
  alignas(kValueVariantAlign) std::byte raw_[kValueVariantSize];
};

}  // namespace common_internal

template <>
struct ArenaTraits<common_internal::ValueVariant> {
  static bool trivially_destructible(
      const common_internal::ValueVariant& value) {
    return (value.flags_ & common_internal::ValueFlags::kNonTrivial) ==
           common_internal::ValueFlags::kNone;
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_VARIANT_H_
