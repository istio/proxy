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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPEINFO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPEINFO_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"  // IWYU pragma: keep
#include "absl/base/config.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"

#if ABSL_HAVE_FEATURE(cxx_rtti)
#define CEL_INTERNAL_HAVE_RTTI 1
#elif defined(__GNUC__) && defined(__GXX_RTTI)
#define CEL_INTERNAL_HAVE_RTTI 1
#elif defined(_MSC_VER) && defined(_CPPRTTI)
#define CEL_INTERNAL_HAVE_RTTI 1
#elif !defined(__GNUC__) && !defined(_MSC_VER)
#define CEL_INTERNAL_HAVE_RTTI 1
#endif

#ifdef CEL_INTERNAL_HAVE_RTTI
#include <typeinfo>
#endif

namespace cel {

class TypeInfo;

template <typename T, typename = void>
struct NativeTypeTraits;

namespace common_internal {

template <typename, typename = void>
struct HasNativeTypeTraitsId : std::false_type {};

template <typename T>
struct HasNativeTypeTraitsId<
    T, std::void_t<decltype(NativeTypeTraits<T>::Id(std::declval<const T&>()))>>
    : std::true_type {};

template <typename T>
static constexpr bool HasNativeTypeTraitsIdV = HasNativeTypeTraitsId<T>::value;

template <typename T, typename = void>
struct HasCelTypeId : std::false_type {};

template <typename T>
struct HasCelTypeId<
    T, std::enable_if_t<std::is_same_v<
           absl::remove_cvref_t<decltype(CelTypeId(std::declval<const T&>()))>,
           TypeInfo>>> : std::true_type {};

}  // namespace common_internal

template <typename T>
TypeInfo TypeId();

template <int&... ExplicitBarrier, typename T>
std::enable_if_t<
    std::conjunction_v<common_internal::HasNativeTypeTraitsId<T>,
                       std::negation<common_internal::HasCelTypeId<T>>>,
    TypeInfo>
TypeId(const T& t [[maybe_unused]]) {
  return NativeTypeTraits<absl::remove_cvref_t<T>>::Id(t);
}

template <int&... ExplicitBarrier, typename T>
std::enable_if_t<
    std::conjunction_v<std::negation<common_internal::HasNativeTypeTraitsId<T>>,
                       std::negation<common_internal::HasCelTypeId<T>>,
                       std::is_final<T>>,
    TypeInfo>
TypeId(const T& t [[maybe_unused]]) {
  return cel::TypeId<absl::remove_cvref_t<T>>();
}

template <int&... ExplicitBarrier, typename T>
std::enable_if_t<
    std::conjunction_v<std::negation<common_internal::HasNativeTypeTraitsId<T>>,
                       common_internal::HasCelTypeId<T>>,
    TypeInfo>
TypeId(const T& t [[maybe_unused]]) {
  return CelTypeId(t);
}

class TypeInfo final {
 public:
  template <typename T>
  ABSL_DEPRECATED("Use cel::TypeId<T>() instead")
  static TypeInfo For() {
    return cel::TypeId<T>();
  }

  template <typename T>
  ABSL_DEPRECATED("Use cel::TypeId(...) instead")
  static TypeInfo Of(const T& type) {
    return cel::TypeId(type);
  }

  TypeInfo() = default;
  TypeInfo(const TypeInfo&) = default;
  TypeInfo& operator=(const TypeInfo&) = default;

  std::string DebugString() const;

  template <typename S>
  friend void AbslStringify(S& sink, TypeInfo type_info) {
    sink.Append(type_info.DebugString());
  }

  friend constexpr bool operator==(TypeInfo lhs, TypeInfo rhs) noexcept {
#ifdef CEL_INTERNAL_HAVE_RTTI
    return lhs.rep_ == rhs.rep_ ||
           (lhs.rep_ != nullptr && rhs.rep_ != nullptr &&
            *lhs.rep_ == *rhs.rep_);
#else
    return lhs.rep_ == rhs.rep_;
#endif
  }

  template <typename H>
  friend H AbslHashValue(H state, TypeInfo id) {
#ifdef CEL_INTERNAL_HAVE_RTTI
    return H::combine(std::move(state),
                      id.rep_ != nullptr ? id.rep_->hash_code() : size_t{0});
#else
    return H::combine(std::move(state), absl::bit_cast<uintptr_t>(id.rep_));
#endif
  }

 private:
  template <typename T>
  friend TypeInfo TypeId();

#ifdef CEL_INTERNAL_HAVE_RTTI
  constexpr explicit TypeInfo(const std::type_info* absl_nullable rep)
      : rep_(rep) {}

  const std::type_info* absl_nullable rep_ = nullptr;
#else
  constexpr explicit TypeInfo(const void* absl_nullable rep) : rep_(rep) {}

  const void* absl_nullable rep_ = nullptr;
#endif
};

#ifndef CEL_INTERNAL_HAVE_RTTI
namespace common_internal {
template <typename T>
struct TypeTag final {
  static constexpr char value = 0;
};
}  // namespace common_internal
#endif

template <typename T>
TypeInfo TypeId() {
  static_assert(!std::is_pointer_v<T>);
  static_assert(std::is_same_v<T, std::decay_t<T>>);
  static_assert(!std::is_same_v<TypeInfo, std::decay_t<T>>);
#ifdef CEL_INTERNAL_HAVE_RTTI
  return TypeInfo(&typeid(T));
#else
  return TypeInfo(&common_internal::TypeTag<T>::value);
#endif
}

inline constexpr bool operator!=(TypeInfo lhs, TypeInfo rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TypeInfo id) {
  return out << id.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPEINFO_H_
