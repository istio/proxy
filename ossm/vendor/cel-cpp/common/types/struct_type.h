// Copyright 2024 Google LLC
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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/type_kind.h"
#include "common/types/basic_struct_type.h"
#include "common/types/message_type.h"
#include "common/types/types.h"

namespace cel {

class Type;
class TypeParameters;

class StructType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kStruct;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructType(MessageType other) : StructType() {
    if (ABSL_PREDICT_TRUE(other)) {
      variant_.emplace<MessageType>(other);
    }
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructType(common_internal::BasicStructType other) : StructType() {
    if (ABSL_PREDICT_TRUE(other)) {
      variant_.emplace<common_internal::BasicStructType>(other);
    }
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructType& operator=(MessageType other) {
    if (ABSL_PREDICT_TRUE(other)) {
      variant_.emplace<MessageType>(other);
    } else {
      variant_.emplace<absl::monostate>();
    }
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructType& operator=(common_internal::BasicStructType other) {
    if (ABSL_PREDICT_TRUE(other)) {
      variant_.emplace<common_internal::BasicStructType>(other);
    } else {
      variant_.emplace<absl::monostate>();
    }
    return *this;
  }

  StructType() = default;
  StructType(const StructType&) = default;
  StructType(StructType&&) = default;
  StructType& operator=(const StructType&) = default;
  StructType& operator=(StructType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  TypeParameters GetParameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  bool IsMessage() const {
    return absl::holds_alternative<MessageType>(variant_);
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, bool> Is() const {
    return IsMessage();
  }

  absl::optional<MessageType> AsMessage() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, absl::optional<MessageType>>
  As() const {
    return AsMessage();
  }

  MessageType GetMessage() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, MessageType> Get() const {
    return GetMessage();
  }

  explicit operator bool() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

 private:
  friend class Type;
  friend class MessageType;
  friend class common_internal::BasicStructType;

  common_internal::TypeVariant ToTypeVariant() const;

  // The default state is well formed but invalid. It can be checked by using
  // the explicit bool operator. This is to allow cases where you want to
  // construct the type and later assign to it before using it. It is required
  // that any instance returned from a function call or passed to a function
  // call must not be in the default state.
  common_internal::StructTypeVariant variant_;
};

inline bool operator==(const StructType& lhs, const StructType& rhs) {
  return static_cast<bool>(lhs) == static_cast<bool>(rhs) &&
         (!static_cast<bool>(lhs) || lhs.name() == rhs.name());
}

inline bool operator!=(const StructType& lhs, const StructType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const StructType& type) {
  return H::combine(std::move(state), static_cast<bool>(type)
                                          ? type.name()
                                          : absl::string_view());
}

inline std::ostream& operator<<(std::ostream& out, const StructType& type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
