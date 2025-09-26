// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_MESSAGE_WRAPPER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_MESSAGE_WRAPPER_H_

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/numeric/bits.h"
#include "base/internal/message_wrapper.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel::interop_internal {
struct MessageWrapperAccess;
}  // namespace cel::interop_internal

namespace google::api::expr::runtime {

// Forward declare to resolve cycle.
class LegacyTypeInfoApis;

// Wrapper type for protobuf messages. This is used to limit internal usages of
// proto APIs and to support working with the proto lite runtime.
//
// Provides operations for checking if down-casting to Message is safe.
class ABSL_DEPRECATED("Use google::protobuf::Message directly") MessageWrapper {
 public:
  // Simple builder class.
  //
  // Wraps a tagged mutable message lite ptr.
  class ABSL_DEPRECATED("Use google::protobuf::Message directly") Builder {
   public:
    explicit Builder(google::protobuf::MessageLite* message)
        : message_ptr_(reinterpret_cast<uintptr_t>(message)) {
      ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(message)) >=
                  kTagSize);
    }
    explicit Builder(google::protobuf::Message* message)
        : message_ptr_(reinterpret_cast<uintptr_t>(message) | kMessageTag) {
      ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(message)) >=
                  kTagSize);
    }

    google::protobuf::MessageLite* message_ptr() const {
      return reinterpret_cast<google::protobuf::MessageLite*>(message_ptr_ & kPtrMask);
    }

    bool HasFullProto() const {
      return (message_ptr_ & kTagMask) == kMessageTag;
    }

    MessageWrapper Build(const LegacyTypeInfoApis* type_info) {
      return MessageWrapper(message_ptr_, type_info);
    }

   private:
    friend class MessageWrapper;

    explicit Builder(uintptr_t message_ptr) : message_ptr_(message_ptr) {}

    uintptr_t message_ptr_;
  };

  static_assert(alignof(google::protobuf::MessageLite) >= 2,
                "Assume that valid MessageLite ptrs have a free low-order bit");
  MessageWrapper() : message_ptr_(0), legacy_type_info_(nullptr) {}

  MessageWrapper(const google::protobuf::MessageLite* message,
                 const LegacyTypeInfoApis* legacy_type_info)
      : message_ptr_(reinterpret_cast<uintptr_t>(message)),
        legacy_type_info_(legacy_type_info) {
    ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(message)) >=
                kTagSize);
  }

  MessageWrapper(const google::protobuf::Message* message,
                 const LegacyTypeInfoApis* legacy_type_info)
      : message_ptr_(reinterpret_cast<uintptr_t>(message) | kMessageTag),
        legacy_type_info_(legacy_type_info) {
    ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(message)) >=
                kTagSize);
  }

  // If true, the message is using the full proto runtime and downcasting to
  // message should be safe.
  bool HasFullProto() const { return (message_ptr_ & kTagMask) == kMessageTag; }

  // Returns the underlying message.
  //
  // Clients must check HasFullProto before downcasting to Message.
  const google::protobuf::MessageLite* message_ptr() const {
    return reinterpret_cast<const google::protobuf::MessageLite*>(message_ptr_ &
                                                        kPtrMask);
  }

  // Type information associated with this message.
  const LegacyTypeInfoApis* legacy_type_info() const {
    return legacy_type_info_;
  }

 private:
  friend struct ::cel::interop_internal::MessageWrapperAccess;

  MessageWrapper(uintptr_t message_ptr,
                 const LegacyTypeInfoApis* legacy_type_info)
      : message_ptr_(message_ptr), legacy_type_info_(legacy_type_info) {}

  Builder ToBuilder() { return Builder(message_ptr_); }

  static constexpr int kTagSize = ::cel::base_internal::kMessageWrapperTagSize;
  static constexpr uintptr_t kTagMask =
      ::cel::base_internal::kMessageWrapperTagMask;
  static constexpr uintptr_t kPtrMask =
      ::cel::base_internal::kMessageWrapperPtrMask;
  static constexpr uintptr_t kMessageTag =
      ::cel::base_internal::kMessageWrapperTagMessageValue;
  uintptr_t message_ptr_;
  const LegacyTypeInfoApis* legacy_type_info_;
};

static_assert(sizeof(MessageWrapper) <= 2 * sizeof(uintptr_t),
              "MessageWrapper must not increase CelValue size.");

}  // namespace google::api::expr::runtime
#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_MESSAGE_WRAPPER_H_
