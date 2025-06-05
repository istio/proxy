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

#include "internal/strings.h"

#include <string>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/lexis.h"
#include "internal/unicode.h"
#include "internal/utf8.h"

namespace cel::internal {

namespace {

constexpr char kHexTable[] = "0123456789abcdef";

constexpr int HexDigitToInt(char x) {
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

constexpr bool IsOctalDigit(char x) { return x >= '0' && x <= '7'; }

// Returns true when following conditions are met:
// - <closing_str> is a suffix of <source>.
// - No other unescaped occurrence of <closing_str> inside <source> (apart from
//   being a suffix).
// Returns false otherwise. If <error> is non-NULL, returns an error message in
// <error>. If <error_offset> is non-NULL, returns the offset in <source> that
// corresponds to the location of the error.
bool CheckForClosingString(absl::string_view source,
                           absl::string_view closing_str, std::string* error) {
  if (closing_str.empty()) return true;

  const char* p = source.data();
  const char* end = p + source.size();

  bool is_closed = false;
  while (p + closing_str.length() <= end) {
    if (*p != '\\') {
      size_t cur_pos = p - source.data();
      bool is_closing =
          absl::StartsWith(absl::ClippedSubstr(source, cur_pos), closing_str);
      if (is_closing && p + closing_str.length() < end) {
        if (error) {
          *error =
              absl::StrCat("String cannot contain unescaped ", closing_str);
        }
        return false;
      }
      is_closed = is_closing && (p + closing_str.length() == end);
    } else {
      p++;  // Read past the escaped character.
    }
    p++;
  }

  if (!is_closed) {
    if (error) {
      *error = absl::StrCat("String must end with ", closing_str);
    }
    return false;
  }

  return true;
}

// ----------------------------------------------------------------------
// CUnescapeInternal()
//    Unescapes C escape sequences and is the reverse of CEscape().
//
//    If 'source' is valid, stores the unescaped string and its size in
//    'dest' and 'dest_len' respectively, and returns true. Otherwise
//    returns false and optionally stores the error description in
//    'error' and the error offset in 'error_offset'. If 'error' is
//    nonempty on return, 'error_offset' is in range [0, str.size()].
//    Set 'error' and 'error_offset' to NULL to disable error reporting.
//
//    'dest' must point to a buffer that is at least as big as 'source'.  The
//    unescaped string cannot grow bigger than the source string since no
//    unescaped sequence is longer than the corresponding escape sequence.
//    'source' and 'dest' must not be the same.
//
// If <closing_str> is non-empty, for <source> to be valid:
// - It must end with <closing_str>.
// - Should not contain any other unescaped occurrence of <closing_str>.
// ----------------------------------------------------------------------
bool UnescapeInternal(absl::string_view source, absl::string_view closing_str,
                      bool is_raw_literal, bool is_bytes_literal,
                      std::string* dest, std::string* error) {
  if (!CheckForClosingString(source, closing_str, error)) {
    return false;
  }

  if (ABSL_PREDICT_FALSE(source.empty())) {
    *dest = std::string();
    return true;
  }

  // Strip off the closing_str from the end before unescaping.
  source = source.substr(0, source.size() - closing_str.size());
  if (!is_bytes_literal) {
    if (!Utf8IsValid(source)) {
      if (error) {
        *error = absl::StrCat("Structurally invalid UTF8 string: ",
                              EscapeBytes(source));
      }
      return false;
    }
  }

  dest->reserve(source.size());

  const char* p = source.data();
  const char* end = p + source.size();
  const char* last_byte = end - 1;

  while (p < end) {
    if (*p != '\\') {
      if (*p != '\r') {
        dest->push_back(*p++);
      } else {
        // All types of newlines in different platforms i.e. '\r', '\n', '\r\n'
        // are replaced with '\n'.
        dest->push_back('\n');
        p++;
        if (p < end && *p == '\n') {
          p++;
        }
      }
    } else {
      if ((p + 1) > last_byte) {
        if (error) {
          *error = is_raw_literal
                       ? "Raw literals cannot end with odd number of \\"
                   : is_bytes_literal ? "Bytes literal cannot end with \\"
                                      : "String literal cannot end with \\";
        }
        return false;
      }
      if (is_raw_literal) {
        // For raw literals, all escapes are valid and those characters ('\\'
        // and the escaped character) come through literally in the string.
        dest->push_back(*p++);
        dest->push_back(*p++);
        continue;
      }
      // Any error that occurs in the escape is accounted to the start of
      // the escape.
      p++;  // Read past the escape character.

      switch (*p) {
        case 'a':
          dest->push_back('\a');
          break;
        case 'b':
          dest->push_back('\b');
          break;
        case 'f':
          dest->push_back('\f');
          break;
        case 'n':
          dest->push_back('\n');
          break;
        case 'r':
          dest->push_back('\r');
          break;
        case 't':
          dest->push_back('\t');
          break;
        case 'v':
          dest->push_back('\v');
          break;
        case '\\':
          dest->push_back('\\');
          break;
        case '?':
          dest->push_back('\?');
          break;  // \?  Who knew?
        case '\'':
          dest->push_back('\'');
          break;
        case '"':
          dest->push_back('\"');
          break;
        case '`':
          dest->push_back('`');
          break;
        case '0':
          ABSL_FALLTHROUGH_INTENDED;
        case '1':
          ABSL_FALLTHROUGH_INTENDED;
        case '2':
          ABSL_FALLTHROUGH_INTENDED;
        case '3': {
          // Octal escape '\ddd': requires exactly 3 octal digits.  Note that
          // the highest valid escape sequence is '\377'.
          // For string literals, octal and hex escape sequences are interpreted
          // as unicode code points, and the related UTF8-encoded character is
          // added to the destination.  For bytes literals, octal and hex
          // escape sequences are interpreted as a single byte value.
          const char* octal_start = p;
          if (p + 2 >= end) {
            if (error) {
              *error =
                  "Illegal escape sequence: Octal escape must be followed by 3 "
                  "octal digits but saw: \\" +
                  std::string(octal_start, end - p);
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          const char* octal_end = p + 2;
          char32_t ch = 0;
          for (; p <= octal_end; ++p) {
            if (IsOctalDigit(*p)) {
              ch = ch * 8 + *p - '0';
            } else {
              if (error) {
                *error =
                    "Illegal escape sequence: Octal escape must be followed by "
                    "3 octal digits but saw: \\" +
                    std::string(octal_start, 3);
              }
              // Error offset was set to the start of the escape above the
              // switch.
              return false;
            }
          }
          p = octal_end;  // p points at last digit.
          if (is_bytes_literal) {
            dest->push_back(static_cast<char>(ch));
          } else {
            Utf8Encode(*dest, ch);
          }
          break;
        }
        case 'x':
          ABSL_FALLTHROUGH_INTENDED;
        case 'X': {
          // Hex escape '\xhh': requires exactly 2 hex digits.
          // For string literals, octal and hex escape sequences are
          // interpreted as unicode code points, and the related UTF8-encoded
          // character is added to the destination.  For bytes literals, octal
          // and hex escape sequences are interpreted as a single byte value.
          const char* hex_start = p;
          if (p + 2 >= end) {
            if (error) {
              *error =
                  "Illegal escape sequence: Hex escape must be followed by 2 "
                  "hex digits but saw: \\" +
                  std::string(hex_start, end - p);
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          char32_t ch = 0;
          const char* hex_end = p + 2;
          for (++p; p <= hex_end; ++p) {
            if (absl::ascii_isxdigit(*p)) {
              ch = (ch << 4) + HexDigitToInt(*p);
            } else {
              if (error) {
                *error =
                    "Illegal escape sequence: Hex escape must be followed by 2 "
                    "hex digits but saw: \\" +
                    std::string(hex_start, 3);
              }
              // Error offset was set to the start of the escape above the
              // switch.
              return false;
            }
          }
          p = hex_end;  // p points at last digit.
          if (is_bytes_literal) {
            dest->push_back(static_cast<char>(ch));
          } else {
            Utf8Encode(*dest, ch);
          }
          break;
        }
        case 'u': {
          if (is_bytes_literal) {
            if (error) {
              *error =
                  std::string(
                      "Illegal escape sequence: Unicode escape sequence \\") +
                  *p + " cannot be used in bytes literals";
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          // \uhhhh => Read 4 hex digits as a code point,
          //           then write it as UTF-8 bytes.
          char32_t cp = 0;
          const char* hex_start = p;
          if (p + 4 >= end) {
            if (error) {
              *error =
                  "Illegal escape sequence: \\u must be followed by 4 hex "
                  "digits but saw: \\" +
                  std::string(hex_start, end - p);
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          for (int i = 0; i < 4; ++i) {
            // Look one char ahead.
            if (absl::ascii_isxdigit(p[1])) {
              cp = (cp << 4) + HexDigitToInt(*++p);  // Advance p.
            } else {
              if (error) {
                *error =
                    "Illegal escape sequence: \\u must be followed by 4 "
                    "hex digits but saw: \\" +
                    std::string(hex_start, 5);
              }
              // Error offset was set to the start of the escape above the
              // switch.
              return false;
            }
          }
          if (!UnicodeIsValid(cp)) {
            if (error) {
              *error = "Illegal escape sequence: Unicode value \\" +
                       std::string(hex_start, 5) + " is invalid";
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          Utf8Encode(*dest, cp);
          break;
        }
        case 'U': {
          if (is_bytes_literal) {
            if (error) {
              *error =
                  std::string(
                      "Illegal escape sequence: Unicode escape sequence \\") +
                  *p + " cannot be used in bytes literals";
            }
            return false;
          }
          // \Uhhhhhhhh => convert 8 hex digits to UTF-8.  Note that the
          // first two digits must be 00: The valid range is
          // '\U00000000' to '\U0010FFFF' (excluding surrogates).
          char32_t cp = 0;
          const char* hex_start = p;
          if (p + 8 >= end) {
            if (error) {
              *error =
                  "Illegal escape sequence: \\U must be followed by 8 hex "
                  "digits but saw: \\" +
                  std::string(hex_start, end - p);
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          for (int i = 0; i < 8; ++i) {
            // Look one char ahead.
            if (absl::ascii_isxdigit(p[1])) {
              cp = (cp << 4) + HexDigitToInt(*++p);
              if (cp > 0x10FFFF) {
                if (error) {
                  *error = "Illegal escape sequence: Value of \\" +
                           std::string(hex_start, 9) +
                           " exceeds Unicode limit (0x0010FFFF)";
                }
                // Error offset was set to the start of the escape above the
                // switch.
                return false;
              }
            } else {
              if (error) {
                *error =
                    "Illegal escape sequence: \\U must be followed by 8 "
                    "hex digits but saw: \\" +
                    std::string(hex_start, 9);
              }
              // Error offset was set to the start of the escape above the
              // switch.
              return false;
            }
          }
          if (!UnicodeIsValid(cp)) {
            if (error) {
              *error = "Illegal escape sequence: Unicode value \\" +
                       std::string(hex_start, 9) + " is invalid";
            }
            // Error offset was set to the start of the escape above the switch.
            return false;
          }
          Utf8Encode(*dest, cp);
          break;
        }
        case '\r':
          ABSL_FALLTHROUGH_INTENDED;
        case '\n': {
          if (error) {
            *error = "Illegal escaped newline";
          }
          // Error offset was set to the start of the escape above the switch.
          return false;
        }
        default: {
          if (error) {
            *error = std::string("Illegal escape sequence: \\") + *p;
          }
          // Error offset was set to the start of the escape above the switch.
          return false;
        }
      }
      p++;  // read past letter we escaped
    }
  }

  dest->shrink_to_fit();

  return true;
}

std::string EscapeInternal(absl::string_view src, bool escape_all_bytes,
                           char escape_quote_char) {
  std::string dest;
  // Worst case size is every byte has to be hex escaped, so 4 char for every
  // byte.
  dest.reserve(src.size() * 4);
  bool last_hex_escape = false;  // true if last output char was \xNN.
  const char* p = src.data();
  const char* end = p + src.size();
  for (; p < end; ++p) {
    unsigned char c = static_cast<unsigned char>(*p);
    bool is_hex_escape = false;
    switch (c) {
      case '\n':
        dest.append("\\n");
        break;
      case '\r':
        dest.append("\\r");
        break;
      case '\t':
        dest.append("\\t");
        break;
      case '\\':
        dest.append("\\\\");
        break;
      case '\'':
        ABSL_FALLTHROUGH_INTENDED;
      case '\"':
        ABSL_FALLTHROUGH_INTENDED;
      case '`':
        // Escape only quote chars that match escape_quote_char.
        if (escape_quote_char == 0 || c == escape_quote_char) {
          dest.push_back('\\');
        }
        dest.push_back(c);
        break;
      default:
        // Note that if we emit \xNN and the src character after that is a hex
        // digit then that digit must be escaped too to prevent it being
        // interpreted as part of the character code by C.
        if ((!escape_all_bytes || c < 0x80) &&
            (!absl::ascii_isprint(c) ||
             (last_hex_escape && absl::ascii_isxdigit(c)))) {
          dest.append("\\x");
          dest.push_back(kHexTable[c / 16]);
          dest.push_back(kHexTable[c % 16]);
          is_hex_escape = true;
        } else {
          dest.push_back(c);
          break;
        }
    }
    last_hex_escape = is_hex_escape;
  }
  dest.shrink_to_fit();
  return dest;
}

bool MayBeTripleQuotedString(absl::string_view str) {
  return (str.size() >= 6 &&
          ((absl::StartsWith(str, "\"\"\"") && absl::EndsWith(str, "\"\"\"")) ||
           (absl::StartsWith(str, "'''") && absl::EndsWith(str, "'''"))));
}

bool MayBeStringLiteral(absl::string_view str) {
  return (str.size() >= 2 && str[0] == str[str.size() - 1] &&
          (str[0] == '\'' || str[0] == '"'));
}

bool MayBeBytesLiteral(absl::string_view str) {
  return (str.size() >= 3 && absl::StartsWithIgnoreCase(str, "b") &&
          str[1] == str[str.size() - 1] && (str[1] == '\'' || str[1] == '"'));
}

bool MayBeRawStringLiteral(absl::string_view str) {
  return (str.size() >= 3 && absl::StartsWithIgnoreCase(str, "r") &&
          str[1] == str[str.size() - 1] && (str[1] == '\'' || str[1] == '"'));
}

bool MayBeRawBytesLiteral(absl::string_view str) {
  return (str.size() >= 4 &&
          (absl::StartsWithIgnoreCase(str, "rb") ||
           absl::StartsWithIgnoreCase(str, "br")) &&
          (str[2] == str[str.size() - 1]) && (str[2] == '\'' || str[2] == '"'));
}

}  // namespace

absl::StatusOr<std::string> UnescapeString(absl::string_view str) {
  std::string out;
  std::string error;
  if (!UnescapeInternal(str, "", false, false, &out, &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid escaped string: ", error));
  }
  return out;
}

absl::StatusOr<std::string> UnescapeBytes(absl::string_view str) {
  std::string out;
  std::string error;
  if (!UnescapeInternal(str, "", false, true, &out, &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid escaped bytes: ", error));
  }
  return out;
}

std::string EscapeString(absl::string_view str) {
  return EscapeInternal(str, true, '\0');
}

std::string EscapeBytes(absl::string_view str, bool escape_all_bytes,
                        char escape_quote_char) {
  std::string escaped_bytes;
  const char* p = str.data();
  const char* end = p + str.size();
  for (; p < end; ++p) {
    unsigned char c = *p;
    if (escape_all_bytes || !absl::ascii_isprint(c)) {
      escaped_bytes += "\\x";
      escaped_bytes += absl::BytesToHexString(absl::string_view(p, 1));
    } else {
      switch (c) {
        // Note that we only handle printable escape characters here.  All
        // unprintable (\n, \r, \t, etc.) are hex escaped above.
        case '\\':
          escaped_bytes += "\\\\";
          break;
        case '\'':
        case '"':
        case '`':
          // Escape only quote chars that match escape_quote_char.
          if (escape_quote_char == 0 || c == escape_quote_char) {
            escaped_bytes += '\\';
          }
          escaped_bytes += c;
          break;
        default:
          escaped_bytes += c;
          break;
      }
    }
  }
  return escaped_bytes;
}

absl::StatusOr<std::string> ParseStringLiteral(absl::string_view str) {
  std::string out;
  bool is_string_literal = MayBeStringLiteral(str);
  bool is_raw_string_literal = MayBeRawStringLiteral(str);
  if (!is_string_literal && !is_raw_string_literal) {
    return absl::InvalidArgumentError("Invalid string literal");
  }

  absl::string_view copy_str = str;
  if (is_raw_string_literal) {
    // Strip off the prefix 'r' from the raw string content before parsing.
    copy_str = absl::ClippedSubstr(copy_str, 1);
  }

  bool is_triple_quoted = MayBeTripleQuotedString(copy_str);
  // Starts after the opening quotes {""", '''} or {", '}.
  int quotes_length = is_triple_quoted ? 3 : 1;
  absl::string_view quotes = copy_str.substr(0, quotes_length);
  copy_str = absl::ClippedSubstr(copy_str, quotes_length);
  std::string error;
  if (!UnescapeInternal(copy_str, quotes, is_raw_string_literal, false, &out,
                        &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid string literal: ", error));
  }
  return out;
}

absl::StatusOr<std::string> ParseBytesLiteral(absl::string_view str) {
  std::string out;
  bool is_bytes_literal = MayBeBytesLiteral(str);
  bool is_raw_bytes_literal = MayBeRawBytesLiteral(str);
  if (!is_bytes_literal && !is_raw_bytes_literal) {
    return absl::InvalidArgumentError("Invalid bytes literal");
  }

  absl::string_view copy_str = str;
  if (is_raw_bytes_literal) {
    // Strip off the prefix {"rb", "br"} from the raw bytes content before
    copy_str = absl::ClippedSubstr(copy_str, 2);
  } else {
    // Strip off the prefix 'b' from the bytes content before parsing.
    copy_str = absl::ClippedSubstr(copy_str, 1);
  }

  bool is_triple_quoted = MayBeTripleQuotedString(copy_str);
  // Starts after the opening quotes {""", '''} or {", '}.
  int quotes_length = is_triple_quoted ? 3 : 1;
  absl::string_view quotes = copy_str.substr(0, quotes_length);
  // Includes the closing quotes.
  copy_str = absl::ClippedSubstr(copy_str, quotes_length);
  std::string error;
  if (!UnescapeInternal(copy_str, quotes, is_raw_bytes_literal, true, &out,
                        &error)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid bytes literal: ", error));
  }
  return out;
}

std::string FormatStringLiteral(absl::string_view str) {
  absl::string_view quote =
      (str.find('"') != str.npos && str.find('\'') == str.npos) ? "'" : "\"";
  return absl::StrCat(quote, EscapeInternal(str, true, quote[0]), quote);
}

std::string FormatStringLiteral(const absl::Cord& str) {
  if (auto flat = str.TryFlat(); flat) {
    return FormatStringLiteral(*flat);
  }
  return FormatStringLiteral(static_cast<std::string>(str));
}

std::string FormatSingleQuotedStringLiteral(absl::string_view str) {
  return absl::StrCat("'", EscapeInternal(str, true, '\''), "'");
}

std::string FormatDoubleQuotedStringLiteral(absl::string_view str) {
  return absl::StrCat("\"", EscapeInternal(str, true, '"'), "\"");
}

std::string FormatBytesLiteral(absl::string_view str) {
  absl::string_view quote =
      (str.find('"') != str.npos && str.find('\'') == str.npos) ? "'" : "\"";
  return absl::StrCat("b", quote, EscapeBytes(str, false, quote[0]), quote);
}

std::string FormatSingleQuotedBytesLiteral(absl::string_view str) {
  return absl::StrCat("b'", EscapeBytes(str, false, '\''), "'");
}

std::string FormatDoubleQuotedBytesLiteral(absl::string_view str) {
  return absl::StrCat("b\"", EscapeBytes(str, false, '"'), "\"");
}

absl::StatusOr<std::string> ParseIdentifier(absl::string_view str) {
  if (!LexisIsIdentifier(str)) {
    return absl::InvalidArgumentError("Invalid identifier");
  }
  return std::string(str);
}

}  // namespace cel::internal
