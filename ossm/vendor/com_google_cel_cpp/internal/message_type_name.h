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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_TYPE_NAME_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_TYPE_NAME_H_

#include <string>
#include <type_traits>

#include "absl/base/no_destructor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel::internal {

// MessageTypeNameFor returns the fully qualified message type name of a
// generated message. This is a portable version which works with the lite
// runtime as well.

template <typename T>
std::enable_if_t<
    std::conjunction_v<std::is_base_of<google::protobuf::MessageLite, T>,
                       std::negation<std::is_base_of<google::protobuf::Message, T>>>,
    absl::string_view>
MessageTypeNameFor() {
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static const absl::NoDestructor<std::string> kTypeName(T().GetTypeName());
  return *kTypeName;
}

template <typename T>
std::enable_if_t<std::is_base_of_v<google::protobuf::Message, T>, absl::string_view>
MessageTypeNameFor() {
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  return T::descriptor()->full_name();
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_TYPE_NAME_H_
