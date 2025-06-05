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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/type_kind.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class Type;
class TypeParameters;

bool IsWellKnownEnumType(
    absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor);

class EnumType final {
 public:
  using element_type = const google::protobuf::EnumDescriptor;

  static constexpr TypeKind kKind = TypeKind::kEnum;

  // Constructs `EnumType` from a pointer to `google::protobuf::EnumDescriptor`. The
  // `google::protobuf::EnumDescriptor` must not be one of the well known enum types we
  // treat specially, if it is behavior is undefined. If you are unsure, you
  // should use `Type::Enum`.
  explicit EnumType(absl::Nullable<const google::protobuf::EnumDescriptor*> descriptor)
      : descriptor_(descriptor) {
    ABSL_DCHECK(descriptor == nullptr || !IsWellKnownEnumType(descriptor))
        << descriptor->full_name();
  }

  EnumType() = default;
  EnumType(const EnumType&) = default;
  EnumType(EnumType&&) = default;
  EnumType& operator=(const EnumType&) = default;
  EnumType& operator=(EnumType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return (*this)->full_name();
  }

  std::string DebugString() const;

  static TypeParameters GetParameters();

  const google::protobuf::EnumDescriptor& operator*() const {
    ABSL_DCHECK(*this);
    return *descriptor_;
  }

  absl::Nonnull<const google::protobuf::EnumDescriptor*> operator->() const {
    ABSL_DCHECK(*this);
    return descriptor_;
  }

  explicit operator bool() const { return descriptor_ != nullptr; }

 private:
  friend struct std::pointer_traits<EnumType>;

  absl::Nullable<const google::protobuf::EnumDescriptor*> descriptor_ = nullptr;
};

inline bool operator==(EnumType lhs, EnumType rhs) {
  return static_cast<bool>(lhs) == static_cast<bool>(rhs) &&
         (!static_cast<bool>(lhs) || lhs.name() == rhs.name());
}

inline bool operator!=(EnumType lhs, EnumType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, EnumType enum_type) {
  return H::combine(std::move(state), static_cast<bool>(enum_type)
                                          ? enum_type.name()
                                          : absl::string_view());
}

inline std::ostream& operator<<(std::ostream& out, EnumType type) {
  return out << type.DebugString();
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::EnumType> {
  using pointer = cel::EnumType;
  using element_type = typename cel::EnumType::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return p.descriptor_;
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_
