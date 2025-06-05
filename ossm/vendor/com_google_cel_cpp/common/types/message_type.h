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
// IWYU pragma: friend "common/types/struct_type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_MESSAGE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_MESSAGE_TYPE_H_

#include <cstddef>
#include <cstdint>
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

bool IsWellKnownMessageType(
    absl::Nonnull<const google::protobuf::Descriptor*> descriptor);

class MessageTypeField;

class MessageType final {
 public:
  using element_type = const google::protobuf::Descriptor;

  static constexpr TypeKind kKind = TypeKind::kStruct;

  // Constructs `MessageType` from a pointer to `google::protobuf::Descriptor`. The
  // `google::protobuf::Descriptor` must not be one of the well known message types we
  // treat specially, if it is behavior is undefined. If you are unsure, you
  // should use `Type::Message`.
  explicit MessageType(absl::Nullable<const google::protobuf::Descriptor*> descriptor)
      : descriptor_(descriptor) {
    ABSL_DCHECK(descriptor == nullptr || !IsWellKnownMessageType(descriptor))
        << descriptor->full_name();
  }

  MessageType() = default;
  MessageType(const MessageType&) = default;
  MessageType(MessageType&&) = default;
  MessageType& operator=(const MessageType&) = default;
  MessageType& operator=(MessageType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return (*this)->full_name();
  }

  std::string DebugString() const;

  static TypeParameters GetParameters();

  const google::protobuf::Descriptor& operator*() const {
    ABSL_DCHECK(*this);
    return *descriptor_;
  }

  absl::Nonnull<const google::protobuf::Descriptor*> operator->() const {
    ABSL_DCHECK(*this);
    return descriptor_;
  }

  explicit operator bool() const { return descriptor_ != nullptr; }

 private:
  friend struct std::pointer_traits<MessageType>;

  absl::Nullable<const google::protobuf::Descriptor*> descriptor_ = nullptr;
};

inline bool operator==(MessageType lhs, MessageType rhs) {
  return static_cast<bool>(lhs) == static_cast<bool>(rhs) &&
         (!static_cast<bool>(lhs) || lhs.name() == rhs.name());
}

inline bool operator!=(MessageType lhs, MessageType rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, MessageType message_type) {
  return H::combine(std::move(state), static_cast<bool>(message_type)
                                          ? message_type.name()
                                          : absl::string_view());
}

inline std::ostream& operator<<(std::ostream& out, MessageType type) {
  return out << type.DebugString();
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::MessageType> {
  using pointer = cel::MessageType;
  using element_type = typename cel::MessageType::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return p.descriptor_;
  }
};

}  // namespace std

namespace cel {

class MessageTypeField final {
 public:
  using element_type = const google::protobuf::FieldDescriptor;

  explicit MessageTypeField(
      absl::Nullable<const google::protobuf::FieldDescriptor*> descriptor)
      : descriptor_(descriptor) {}

  MessageTypeField() = default;
  MessageTypeField(const MessageTypeField&) = default;
  MessageTypeField(MessageTypeField&&) = default;
  MessageTypeField& operator=(const MessageTypeField&) = default;
  MessageTypeField& operator=(MessageTypeField&&) = default;

  std::string DebugString() const;

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return (*this)->name();
  }

  int32_t number() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return (*this)->number();
  }

  Type GetType() const;

  const google::protobuf::FieldDescriptor& operator*() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return *descriptor_;
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return descriptor_;
  }

  explicit operator bool() const { return descriptor_ != nullptr; }

 private:
  friend struct std::pointer_traits<MessageTypeField>;

  absl::Nullable<const google::protobuf::FieldDescriptor*> descriptor_ = nullptr;
};

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::MessageTypeField> {
  using pointer = cel::MessageTypeField;
  using element_type = typename cel::MessageTypeField::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return p.descriptor_;
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_MESSAGE_TYPE_H_
