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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_

#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel::internal {

// Returns true if the given UTF-8 encoded string is not malformed, false
// otherwise.
bool Utf8IsValid(absl::string_view str);
bool Utf8IsValid(const absl::Cord& str);

// Returns the number of Unicode code points in the UTF-8 encoded string.
//
// If there are any invalid bytes, they will each be counted as an invalid code
// point.
size_t Utf8CodePointCount(absl::string_view str);
size_t Utf8CodePointCount(const absl::Cord& str);

// Validates the given UTF-8 encoded string. The first return value is the
// number of code points and its meaning depends on the second return value. If
// the second return value is true the entire string is not malformed and the
// first return value is the number of code points. If the second return value
// is false the string is malformed and the first return value is the number of
// code points up until the malformed sequence was encountered.
std::pair<size_t, bool> Utf8Validate(absl::string_view str);
std::pair<size_t, bool> Utf8Validate(const absl::Cord& str);

// Decodes the next code point, returning the decoded code point and the number
// of code units (a.k.a. bytes) consumed. In the event that an invalid code unit
// sequence is returned the replacement character, U+FFFD, is returned with a
// code unit count of 1. As U+FFFD requires 3 code units when encoded, this can
// be used to differentiate valid input from malformed input.
std::pair<char32_t, size_t> Utf8Decode(absl::string_view str);
std::pair<char32_t, size_t> Utf8Decode(const absl::Cord::CharIterator& it);

// Encodes the given code point and appends it to the buffer. If the code point
// is an unpaired surrogate or outside of the valid Unicode range it is replaced
// with the replacement character, U+FFFD.
size_t Utf8Encode(std::string& buffer, char32_t code_point);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_UTF8_H_
