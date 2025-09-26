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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STRINGS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STRINGS_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel::internal {

// Expand escaped characters according to CEL escaping rules.
// This is for raw strings with no quoting.
absl::StatusOr<std::string> UnescapeString(absl::string_view str);

// Expand escaped characters according to CEL escaping rules.
// Rules for bytes values are slightly different than those for strings.  This
// is for raw literals with no quoting.
absl::StatusOr<std::string> UnescapeBytes(absl::string_view str);

// Escape a string without quoting it. All quote characters are escaped.
std::string EscapeString(absl::string_view str);

// Escape a bytes value without quoting it.  Escaped bytes use hex escapes.
// If <escape_all_bytes> is true then all bytes are escaped.  Otherwise only
// unprintable bytes and escape/quote characters are escaped.
// If <escape_quote_char> is not 0, then quotes that do not match are not
// escaped.
std::string EscapeBytes(absl::string_view str, bool escape_all_bytes = false,
                        char escape_quote_char = '\0');

// Unquote and unescape a quoted CEL string literal (of the form '...',
// "...", r'...' or r"...").
// If an error occurs and <error_string> is not NULL, then it is populated with
// the relevant error message. If <error_offset> is not NULL, it is populated
// with the offset in <str> at which the invalid input occurred.
absl::StatusOr<std::string> ParseStringLiteral(absl::string_view str);

// Unquote and unescape a CEL bytes literal (of the form b'...',
// b"...", rb'...', rb"...", br'...' or br"...").
// If an error occurs and <error_string> is not NULL, then it is populated with
// the relevant error message. If <error_offset> is not NULL, it is populated
// with the offset in <str> at which the invalid input occurred.
absl::StatusOr<std::string> ParseBytesLiteral(absl::string_view str);

// Return a quoted and escaped CEL string literal for <str>.
// May choose to quote with ' or " to produce nicer output.
std::string FormatStringLiteral(absl::string_view str);
std::string FormatStringLiteral(const absl::Cord& str);

// Return a quoted and escaped CEL string literal for <str>.
// Always uses single quotes.
std::string FormatSingleQuotedStringLiteral(absl::string_view str);

// Return a quoted and escaped CEL string literal for <str>.
// Always uses double quotes.
std::string FormatDoubleQuotedStringLiteral(absl::string_view str);

// Return a quoted and escaped CEL bytes literal for <str>.
// Prefixes with b and may choose to quote with ' or " to produce nicer output.
std::string FormatBytesLiteral(absl::string_view str);

// Return a quoted and escaped CEL bytes literal for <str>.
// Prefixes with b and always uses single quotes.
std::string FormatSingleQuotedBytesLiteral(absl::string_view str);

// Return a quoted and escaped CEL bytes literal for <str>.
// Prefixes with b and always uses double quotes.
std::string FormatDoubleQuotedBytesLiteral(absl::string_view str);

// Parse a CEL identifier.
absl::StatusOr<std::string> ParseIdentifier(absl::string_view str);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STRINGS_H_
