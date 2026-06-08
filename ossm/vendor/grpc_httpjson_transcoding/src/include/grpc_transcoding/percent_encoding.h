/* Copyright 2022 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GRPC_TRANSCODING_PERCENT_ENCODING_H_
#define GRPC_TRANSCODING_PERCENT_ENCODING_H_

#include <string>

#include "absl/strings/string_view.h"

namespace google {
namespace grpc {
namespace transcoding {

enum class UrlUnescapeSpec {
  // URL path parameters will not decode RFC 6570 reserved characters.
  // This is the default behavior.
  kAllCharactersExceptReserved = 0,
  // URL path parameters will be fully URI-decoded except in
  // cases of single segment matches in reserved expansion, where "%2F" will be
  // left encoded.
  kAllCharactersExceptSlash,
  // URL path parameters will be fully URI-decoded.
  kAllCharacters,
};

inline bool IsReservedChar(char c) {
  // Reserved characters according to RFC 6570
  switch (c) {
    case '!':
    case '#':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case '/':
    case ':':
    case ';':
    case '=':
    case '?':
    case '@':
    case '[':
    case ']':
      return true;
    default:
      return false;
  }
}

// Check if an ASCII character is a hex digit.  We can't use ctype's
// isxdigit() because it is affected by locale. This function is applied
// to the escaped characters in a url, not to natural-language
// strings, so locale should not be taken into account.
inline bool ascii_isxdigit(char c) {
  return ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F') ||
         ('0' <= c && c <= '9');
}

inline int hex_digit_to_int(char c) {
  /* Assume ASCII. */
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

// This is a helper function for UrlUnescapeString. It takes a string and
// the index of where we are within that string.
//
// The function returns true if the next three characters are of the format:
// "%[0-9A-Fa-f]{2}".
//
// If the next three characters are an escaped character then this function will
// also return what character is escaped.
//
// If unescape_plus is true, unescape '+' to space.
//
// return value: 0: not unescaped, >0: unescaped, number of used original
// characters.
//
inline int GetEscapedChar(absl::string_view src, size_t i,
                          UrlUnescapeSpec unescape_spec, bool unescape_plus,
                          char* out) {
  if (unescape_plus && src[i] == '+') {
    *out = ' ';
    return 1;
  }
  if (i + 2 < src.size() && src[i] == '%') {
    if (ascii_isxdigit(src[i + 1]) && ascii_isxdigit(src[i + 2])) {
      char c =
          (hex_digit_to_int(src[i + 1]) << 4) | hex_digit_to_int(src[i + 2]);
      switch (unescape_spec) {
        case UrlUnescapeSpec::kAllCharactersExceptReserved:
          if (IsReservedChar(c)) {
            return 0;
          }
          break;
        case UrlUnescapeSpec::kAllCharactersExceptSlash:
          if (c == '/') {
            return 0;
          }
          break;
        case UrlUnescapeSpec::kAllCharacters:
          break;
      }
      *out = c;
      return 3;
    }
  }
  return 0;
}

inline bool IsUrlEscapedString(absl::string_view part,
                               UrlUnescapeSpec unescape_spec,
                               bool unescape_plus) {
  char ch = '\0';
  for (size_t i = 0; i < part.size(); ++i) {
    if (GetEscapedChar(part, i, unescape_spec, unescape_plus, &ch) > 0) {
      return true;
    }
  }
  return false;
}

inline bool IsUrlEscapedString(absl::string_view part) {
  return IsUrlEscapedString(part, UrlUnescapeSpec::kAllCharacters, false);
}

// Unescapes string 'part' and returns the unescaped string. Reserved characters
// (as specified in RFC 6570) are not escaped if unescape_reserved_chars is
// false.
inline std::string UrlUnescapeString(absl::string_view part,
                                     UrlUnescapeSpec unescape_spec,
                                     bool unescape_plus) {
  // Check whether we need to escape at all.
  if (!IsUrlEscapedString(part, unescape_spec, unescape_plus)) {
    return std::string(part);
  }

  std::string unescaped;
  char ch = '\0';
  unescaped.resize(part.size());

  char* begin = &(unescaped)[0];
  char* p = begin;

  for (size_t i = 0; i < part.size();) {
    int skip = GetEscapedChar(part, i, unescape_spec, unescape_plus, &ch);
    if (skip > 0) {
      *p++ = ch;
      i += skip;
    } else {
      *p++ = part[i];
      i += 1;
    }
  }

  unescaped.resize(p - begin);
  return unescaped;
}

inline std::string UrlUnescapeString(absl::string_view part) {
  return UrlUnescapeString(part, UrlUnescapeSpec::kAllCharacters, false);
}

}  // namespace transcoding
}  // namespace grpc
}  // namespace google

#endif  // GRPC_TRANSCODING_PERCENT_ENCODING_H_
