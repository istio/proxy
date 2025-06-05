// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_INTERNAL_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_PARSER_INTERNAL_OPTIONS_H_

namespace cel_parser_internal {

inline constexpr int kDefaultErrorRecoveryLimit = 12;
inline constexpr int kDefaultMaxRecursionDepth = 32;
inline constexpr int kExpressionSizeCodepointLimit = 100'000;
inline constexpr int kDefaultErrorRecoveryTokenLookaheadLimit = 512;
inline constexpr bool kDefaultAddMacroCalls = false;

}  // namespace cel_parser_internal

#endif  // THIRD_PARTY_CEL_CPP_PARSER_INTERNAL_OPTIONS_H_
