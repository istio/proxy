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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_

#include <type_traits>

#include "google/protobuf/struct.pb.h"
#include "absl/meta/type_traits.h"
#include "google/protobuf/generated_enum_util.h"

namespace cel::common_internal {

template <typename T, typename U = absl::remove_cv_t<T>>
inline constexpr bool kIsWellKnownEnumType =
    std::is_same<google::protobuf::NullValue, U>::value;

template <typename T, typename U = absl::remove_cv_t<T>>
inline constexpr bool kIsGeneratedEnum = google::protobuf::is_proto_enum<U>::value;

template <typename T, typename U, typename R = void>
using EnableIfWellKnownEnum = std::enable_if_t<
    kIsWellKnownEnumType<T> && std::is_same<absl::remove_cv_t<T>, U>::value, R>;

template <typename T, typename R = void>
using EnableIfGeneratedEnum = std::enable_if_t<
    absl::conjunction<
        std::bool_constant<kIsGeneratedEnum<T>>,
        absl::negation<std::bool_constant<kIsWellKnownEnumType<T>>>>::value,
    R>;

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
