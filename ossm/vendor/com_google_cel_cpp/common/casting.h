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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CASTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CASTING_H_

#include "absl/base/attributes.h"
#include "common/internal/casting.h"

namespace cel {

// `InstanceOf<To>(const From&)` determines whether `From` holds or is `To`.
//
// `To` must be a plain non-union class type that is not qualified.
//
// We expose `InstanceOf` this way to avoid ADL.
//
// Example:
//
// if (InstanceOf<Subclass>(superclass)) {
//   Cast<Subclass>(superclass).SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED("Use Is member functions instead.")
inline constexpr common_internal::InstanceOfImpl<To> InstanceOf{};

// `Cast<To>(From)` is a "checked cast". In debug builds an assertion is emitted
// which verifies `From` is an instance-of `To`. In non-debug builds, invalid
// casts are undefined behavior.
//
// We expose `Cast` this way to avoid ADL.
//
// Example:
//
// if (InstanceOf<Subclass>(superclass)) {
//   Cast<Subclass>(superclass).SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED(
    "Use explicit conversion functions instead through static_cast.")
inline constexpr common_internal::CastImpl<To> Cast{};

// `As<To>(From)` is a "checking cast". The result is explicitly convertible to
// `bool`, such that it can be used with `if` statements. The result can be
// accessed with `operator*` or `operator->`. The return type should be treated
// as an implementation detail, with no assumptions on the concrete type. You
// should use `auto`.
//
// `As` is analogous to the paradigm `if (InstanceOf<B>(a)) Cast<B>(a)`.
//
// We expose `As` this way to avoid ADL.
//
// Example:
//
// if (auto subclass = As<Subclass>(superclass); subclass) {
//   subclass->SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED("Use As member functions instead.")
inline constexpr common_internal::AsImpl<To> As{};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INSTANCE_OF_H_
