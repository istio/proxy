/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_STUBS_STRUTIL_H__
#define GOOGLE_PROTOBUF_STUBS_STRUTIL_H__

#include <cstdlib>
#include <cstring>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "google/protobuf/stubs/port.h"

namespace google {
namespace protobuf {

#if defined(_MSC_VER) && _MSC_VER < 1800
#define strtoll _strtoi64
#define strtoull _strtoui64
#elif defined(__DECCXX) && defined(__osf__)
// HP C++ on Tru64 does not have strtoll, but strtol is already 64-bit.
#define strtoll strtol
#define strtoull strtoul
#endif

// ----------------------------------------------------------------------
// ascii_isalnum()
//    Check if an ASCII character is alphanumeric.  We can't use ctype's
//    isalnum() because it is affected by locale.  This function is applied
//    to identifiers in the protocol buffer language, not to natural-language
//    strings, so locale should not be taken into account.
// ascii_isdigit()
//    Like above, but only accepts digits.
// ascii_isspace()
//    Check if the character is a space character.
// ----------------------------------------------------------------------

inline bool ascii_isalnum(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9');
}

inline bool ascii_isdigit(char c) { return ('0' <= c && c <= '9'); }

inline bool ascii_isspace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
         c == '\r';
}

inline bool ascii_isupper(char c) { return c >= 'A' && c <= 'Z'; }

inline bool ascii_islower(char c) { return c >= 'a' && c <= 'z'; }

inline char ascii_toupper(char c) {
  return ascii_islower(c) ? c - ('a' - 'A') : c;
}

inline char ascii_tolower(char c) {
  return ascii_isupper(c) ? c + ('a' - 'A') : c;
}

inline int hex_digit_to_int(char c) {
  /* Assume ASCII. */
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

// ----------------------------------------------------------------------
// HasPrefixString()
//    Check if a string begins with a given prefix.
// StripPrefixString()
//    Given a string and a putative prefix, returns the string minus the
//    prefix string if the prefix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool HasPrefixString(absl::string_view str, absl::string_view prefix) {
  return str.size() >= prefix.size() &&
         memcmp(str.data(), prefix.data(), prefix.size()) == 0;
}

inline std::string StripPrefixString(const std::string& str,
                                     const std::string& prefix) {
  if (HasPrefixString(str, prefix)) {
    return str.substr(prefix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// HasSuffixString()
//    Return true if str ends in suffix.
// StripSuffixString()
//    Given a string and a putative suffix, returns the string minus the
//    suffix string if the suffix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool HasSuffixString(absl::string_view str, absl::string_view suffix) {
  return str.size() >= suffix.size() &&
         memcmp(str.data() + str.size() - suffix.size(), suffix.data(),
                suffix.size()) == 0;
}

inline std::string StripSuffixString(const std::string& str,
                                     const std::string& suffix) {
  if (HasSuffixString(str, suffix)) {
    return str.substr(0, str.size() - suffix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// ReplaceCharacters
//    Replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
//    Good for keeping html characters or protocol characters (\t) out
//    of places where they might cause a problem.
// StripWhitespace
//    Removes whitespaces from both ends of the given string.
// ----------------------------------------------------------------------
void ReplaceCharacters(std::string* s, const char* remove, char replacewith);

void StripWhitespace(std::string* s);

// ----------------------------------------------------------------------
// LowerString()
// UpperString()
// ToUpper()
//    Convert the characters in "s" to lowercase or uppercase.  ASCII-only:
//    these functions intentionally ignore locale because they are applied to
//    identifiers used in the Protocol Buffer language, not to natural-language
//    strings.
// ----------------------------------------------------------------------

inline void LowerString(std::string* s) {
  std::string::iterator end = s->end();
  for (std::string::iterator i = s->begin(); i != end; ++i) {
    // tolower() changes based on locale.  We don't want this!
    if ('A' <= *i && *i <= 'Z') *i += 'a' - 'A';
  }
}

inline void UpperString(std::string* s) {
  std::string::iterator end = s->end();
  for (std::string::iterator i = s->begin(); i != end; ++i) {
    // toupper() changes based on locale.  We don't want this!
    if ('a' <= *i && *i <= 'z') *i += 'A' - 'a';
  }
}

inline void ToUpper(std::string* s) { UpperString(s); }

inline std::string ToUpper(const std::string& s) {
  std::string out = s;
  UpperString(&out);
  return out;
}

// ----------------------------------------------------------------------
// StringReplace()
//    Give me a string and two patterns "old" and "new", and I replace
//    the first instance of "old" in the string with "new", if it
//    exists.  RETURN a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

std::string StringReplace(const std::string& s, const std::string& oldsub,
                          const std::string& newsub, bool replace_all);

// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.  If there are consecutive delimiters, this function skips
//    over all of them.
// ----------------------------------------------------------------------
void SplitStringUsing(absl::string_view full, const char* delim,
                      std::vector<std::string>* res);

// Split a string using one or more byte delimiters, presented
// as a nul-terminated c string. Append the components to 'result'.
// If there are consecutive delimiters, this function will return
// corresponding empty strings.  If you want to drop the empty
// strings, try SplitStringUsing().
//
// If "full" is the empty string, yields an empty string as the only value.
// ----------------------------------------------------------------------
void SplitStringAllowEmpty(absl::string_view full, const char* delim,
                           std::vector<std::string>* result);

// ----------------------------------------------------------------------
// Split()
//    Split a string using a character delimiter.
// ----------------------------------------------------------------------
inline std::vector<std::string> Split(absl::string_view full, const char* delim,
                                      bool skip_empty = true) {
  std::vector<std::string> result;
  if (skip_empty) {
    SplitStringUsing(full, delim, &result);
  } else {
    SplitStringAllowEmpty(full, delim, &result);
  }
  return result;
}

// ----------------------------------------------------------------------
// JoinStrings()
//    These methods concatenate a vector of strings into a C++ string, using
//    the C-string "delim" as a separator between components. There are two
//    flavors of the function, one flavor returns the concatenated string,
//    another takes a pointer to the target string. In the latter case the
//    target string is cleared and overwritten.
// ----------------------------------------------------------------------
void JoinStrings(const std::vector<std::string>& components, const char* delim,
                 std::string* result);

inline std::string JoinStrings(const std::vector<std::string>& components,
                               const char* delim) {
  std::string result;
  JoinStrings(components, delim, &result);
  return result;
}

// ----------------------------------------------------------------------
// UnescapeCEscapeSequences()
//    Copies "source" to "dest", rewriting C-style escape sequences
//    -- '\n', '\r', '\\', '\ooo', etc -- to their ASCII
//    equivalents.  "dest" must be sufficiently large to hold all
//    the characters in the rewritten string (i.e. at least as large
//    as strlen(source) + 1 should be safe, since the replacements
//    are always shorter than the original escaped sequences).  It's
//    safe for source and dest to be the same.  RETURNS the length
//    of dest.
//
//    It allows hex sequences \xhh, or generally \xhhhhh with an
//    arbitrary number of hex digits, but all of them together must
//    specify a value of a single byte (e.g. \x0045 is equivalent
//    to \x45, and \x1234 is erroneous).
//
//    It also allows escape sequences of the form \uhhhh (exactly four
//    hex digits, upper or lower case) or \Uhhhhhhhh (exactly eight
//    hex digits, upper or lower case) to specify a Unicode code
//    point. The dest array will contain the UTF8-encoded version of
//    that code-point (e.g., if source contains \u2019, then dest will
//    contain the three bytes 0xE2, 0x80, and 0x99).
//
//    Errors: In the first form of the call, errors are reported with
//    LOG(ERROR). The same is true for the second form of the call if
//    the pointer to the string std::vector is nullptr; otherwise, error
//    messages are stored in the std::vector. In either case, the effect on
//    the dest array is not defined, but rest of the source will be
//    processed.
//    ----------------------------------------------------------------------

int UnescapeCEscapeSequences(const char* source, char* dest);
int UnescapeCEscapeSequences(const char* source, char* dest,
                             std::vector<std::string>* errors);

// ----------------------------------------------------------------------
// UnescapeCEscapeString()
//    This does the same thing as UnescapeCEscapeSequences, but creates
//    a new string. The caller does not need to worry about allocating
//    a dest buffer. This should be used for non performance critical
//    tasks such as printing debug messages. It is safe for src and dest
//    to be the same.
//
//    The second call stores its errors in a supplied string vector.
//    If the string vector pointer is nullptr, it reports the errors with LOG().
//
//    In the first and second calls, the length of dest is returned. In the
//    the third call, the new string is returned.
// ----------------------------------------------------------------------

int UnescapeCEscapeString(const std::string& src, std::string* dest);
int UnescapeCEscapeString(const std::string& src, std::string* dest,
                          std::vector<std::string>* errors);
std::string UnescapeCEscapeString(const std::string& src);

// ----------------------------------------------------------------------
// CEscape()
//    Escapes 'src' using C-style escape sequences and returns the resulting
//    string.
//
//    Escaped chars: \n, \r, \t, ", ', \, and !isprint().
// ----------------------------------------------------------------------
std::string CEscape(const std::string& src);

// ----------------------------------------------------------------------
// CEscapeAndAppend()
//    Escapes 'src' using C-style escape sequences, and appends the escaped
//    string to 'dest'.
// ----------------------------------------------------------------------
void CEscapeAndAppend(absl::string_view src, std::string* dest);

namespace strings {
// Like CEscape() but does not escape bytes with the upper bit set.
std::string Utf8SafeCEscape(const std::string& src);

// Like CEscape() but uses hex (\x) escapes instead of octals.
std::string CHexEscape(const std::string& src);
}  // namespace strings

// ----------------------------------------------------------------------
// strto32()
// strtou32()
// strto64()
// strtou64()
//    Architecture-neutral plug compatible replacements for strtol() and
//    strtoul().  Long's have different lengths on ILP-32 and LP-64
//    platforms, so using these is safer, from the point of view of
//    overflow behavior, than using the standard libc functions.
// ----------------------------------------------------------------------
int32_t strto32_adaptor(const char* nptr, char** endptr, int base);
uint32_t strtou32_adaptor(const char* nptr, char** endptr, int base);

inline int32_t strto32(const char* nptr, char** endptr, int base) {
  if (sizeof(int32_t) == sizeof(long))
    return strtol(nptr, endptr, base);
  else
    return strto32_adaptor(nptr, endptr, base);
}

inline uint32_t strtou32(const char* nptr, char** endptr, int base) {
  if (sizeof(uint32_t) == sizeof(unsigned long))
    return strtoul(nptr, endptr, base);
  else
    return strtou32_adaptor(nptr, endptr, base);
}

// For now, long long is 64-bit on all the platforms we care about, so these
// functions can simply pass the call to strto[u]ll.
inline int64_t strto64(const char* nptr, char** endptr, int base) {
  static_assert(sizeof(int64_t) == sizeof(long long),
                "sizeof int64_t is not sizeof long long");
  return strtoll(nptr, endptr, base);
}

inline uint64_t strtou64(const char* nptr, char** endptr, int base) {
  static_assert(sizeof(uint64_t) == sizeof(unsigned long long),
                "sizeof uint64_t is not sizeof unsigned long long");
  return strtoull(nptr, endptr, base);
}

// ----------------------------------------------------------------------
// safe_strtob()
// safe_strto32()
// safe_strtou32()
// safe_strto64()
// safe_strtou64()
// safe_strtof()
// safe_strtod()
// ----------------------------------------------------------------------
bool safe_strtob(absl::string_view str, bool* value);

bool safe_strto32(const std::string& str, int32_t* value);
bool safe_strtou32(const std::string& str, uint32_t* value);
inline bool safe_strto32(const char* str, int32_t* value) {
  return safe_strto32(std::string(str), value);
}
inline bool safe_strto32(absl::string_view str, int32_t* value) {
  return safe_strto32(std::string(str), value);
}
inline bool safe_strtou32(const char* str, uint32_t* value) {
  return safe_strtou32(std::string(str), value);
}
inline bool safe_strtou32(absl::string_view str, uint32_t* value) {
  return safe_strtou32(std::string(str), value);
}

bool safe_strto64(const std::string& str, int64_t* value);
bool safe_strtou64(const std::string& str, uint64_t* value);
inline bool safe_strto64(const char* str, int64_t* value) {
  return safe_strto64(std::string(str), value);
}
inline bool safe_strto64(absl::string_view str, int64_t* value) {
  return safe_strto64(std::string(str), value);
}
inline bool safe_strtou64(const char* str, uint64_t* value) {
  return safe_strtou64(std::string(str), value);
}
inline bool safe_strtou64(absl::string_view str, uint64_t* value) {
  return safe_strtou64(std::string(str), value);
}

bool safe_strtof(const char* str, float* value);
bool safe_strtod(const char* str, double* value);
inline bool safe_strtof(const std::string& str, float* value) {
  return safe_strtof(str.c_str(), value);
}
inline bool safe_strtod(const std::string& str, double* value) {
  return safe_strtod(str.c_str(), value);
}
inline bool safe_strtof(absl::string_view str, float* value) {
  return safe_strtof(std::string(str), value);
}
inline bool safe_strtod(absl::string_view str, double* value) {
  return safe_strtod(std::string(str), value);
}

// ----------------------------------------------------------------------
// FastIntToBuffer()
// FastHexToBuffer()
// FastHex64ToBuffer()
// FastHex32ToBuffer()
// FastTimeToBuffer()
//    These are intended for speed.  FastIntToBuffer() assumes the
//    integer is non-negative.  FastHexToBuffer() puts output in
//    hex rather than decimal.  FastTimeToBuffer() puts the output
//    into RFC822 format.
//
//    FastHex64ToBuffer() puts a 64-bit unsigned value in hex-format,
//    padded to exactly 16 bytes (plus one byte for '\0')
//
//    FastHex32ToBuffer() puts a 32-bit unsigned value in hex-format,
//    padded to exactly 8 bytes (plus one byte for '\0')
//
//       All functions take the output buffer as an arg.
//    They all return a pointer to the beginning of the output,
//    which may not be the beginning of the input buffer.
// ----------------------------------------------------------------------

// Suggested buffer size for FastToBuffer functions.  Also works with
// DoubleToBuffer() and FloatToBuffer().
static const int kFastToBufferSize = 32;

char* FastInt32ToBuffer(int32_t i, char* buffer);
char* FastInt64ToBuffer(int64_t i, char* buffer);
char* FastUInt32ToBuffer(uint32_t i, char* buffer);  // inline below
char* FastUInt64ToBuffer(uint64_t i, char* buffer);  // inline below
char* FastHexToBuffer(int i, char* buffer);
char* FastHex64ToBuffer(uint64_t i, char* buffer);
char* FastHex32ToBuffer(uint32_t i, char* buffer);

// at least 22 bytes long
inline char* FastIntToBuffer(int i, char* buffer) {
  return (sizeof(i) == 4 ? FastInt32ToBuffer(i, buffer)
                         : FastInt64ToBuffer(i, buffer));
}
inline char* FastUIntToBuffer(unsigned int i, char* buffer) {
  return (sizeof(i) == 4 ? FastUInt32ToBuffer(i, buffer)
                         : FastUInt64ToBuffer(i, buffer));
}
inline char* FastLongToBuffer(long i, char* buffer) {
  return (sizeof(i) == 4 ? FastInt32ToBuffer(i, buffer)
                         : FastInt64ToBuffer(i, buffer));
}
inline char* FastULongToBuffer(unsigned long i, char* buffer) {
  return (sizeof(i) == 4 ? FastUInt32ToBuffer(i, buffer)
                         : FastUInt64ToBuffer(i, buffer));
}

// ----------------------------------------------------------------------
// FastInt32ToBufferLeft()
// FastUInt32ToBufferLeft()
// FastInt64ToBufferLeft()
// FastUInt64ToBufferLeft()
//
// Like the Fast*ToBuffer() functions above, these are intended for speed.
// Unlike the Fast*ToBuffer() functions, however, these functions write
// their output to the beginning of the buffer (hence the name, as the
// output is left-aligned).  The caller is responsible for ensuring that
// the buffer has enough space to hold the output.
//
// Returns a pointer to the end of the string (i.e. the null character
// terminating the string).
// ----------------------------------------------------------------------

char* FastInt32ToBufferLeft(int32_t i, char* buffer);
char* FastUInt32ToBufferLeft(uint32_t i, char* buffer);
char* FastInt64ToBufferLeft(int64_t i, char* buffer);
char* FastUInt64ToBufferLeft(uint64_t i, char* buffer);

// Just define these in terms of the above.
inline char* FastUInt32ToBuffer(uint32_t i, char* buffer) {
  FastUInt32ToBufferLeft(i, buffer);
  return buffer;
}
inline char* FastUInt64ToBuffer(uint64_t i, char* buffer) {
  FastUInt64ToBufferLeft(i, buffer);
  return buffer;
}

inline std::string SimpleBtoa(bool value) { return value ? "true" : "false"; }

// ----------------------------------------------------------------------
// SimpleItoa()
//    Description: converts an integer to a string.
//
//    Return value: string
// ----------------------------------------------------------------------
std::string SimpleItoa(int i);
std::string SimpleItoa(unsigned int i);
std::string SimpleItoa(long i);
std::string SimpleItoa(unsigned long i);
std::string SimpleItoa(long long i);
std::string SimpleItoa(unsigned long long i);

// ----------------------------------------------------------------------
// SimpleDtoa()
// SimpleFtoa()
// DoubleToBuffer()
// FloatToBuffer()
//    Description: converts a double or float to a string which, if
//    passed to NoLocaleStrtod(), will produce the exact same original double
//    (except in case of NaN; all NaNs are considered the same value).
//    We try to keep the string short but it's not guaranteed to be as
//    short as possible.
//
//    DoubleToBuffer() and FloatToBuffer() write the text to the given
//    buffer and return it.  The buffer must be at least
//    kDoubleToBufferSize bytes for doubles and kFloatToBufferSize
//    bytes for floats.  kFastToBufferSize is also guaranteed to be large
//    enough to hold either.
//
//    Return value: string
// ----------------------------------------------------------------------
std::string SimpleDtoa(double value);
std::string SimpleFtoa(float value);

char* DoubleToBuffer(double i, char* buffer);
char* FloatToBuffer(float i, char* buffer);

// In practice, doubles should never need more than 24 bytes and floats
// should never need more than 14 (including null terminators), but we
// overestimate to be safe.
static const int kDoubleToBufferSize = 32;
static const int kFloatToBufferSize = 24;

namespace strings {

using Hex = absl::Hex;

}  // namespace strings

// ----------------------------------------------------------------------
// ToHex()
//    Return a lower-case hex string representation of the given integer.
// ----------------------------------------------------------------------
std::string ToHex(uint64_t num);

// ----------------------------------------------------------------------
// GlobalReplaceSubstring()
//    Replaces all instances of a substring in a string.  Does nothing
//    if 'substring' is empty.  Returns the number of replacements.
//
//    NOTE: The string pieces must not overlap s.
// ----------------------------------------------------------------------
int GlobalReplaceSubstring(const std::string& substring,
                           const std::string& replacement, std::string* s);

// ----------------------------------------------------------------------
// Base64Unescape()
//    Converts "src" which is encoded in Base64 to its binary equivalent and
//    writes it to "dest". If src contains invalid characters, dest is cleared
//    and the function returns false. Returns true on success.
// ----------------------------------------------------------------------
bool Base64Unescape(absl::string_view src, std::string* dest);

// ----------------------------------------------------------------------
// WebSafeBase64Unescape()
//    This is a variation of Base64Unescape which uses '-' instead of '+', and
//    '_' instead of '/'. src is not null terminated, instead specify len. I
//    recommend that slen<szdest, but we honor szdest anyway.
//    RETURNS the length of dest, or -1 if src contains invalid chars.

//    The variation that stores into a string clears the string first, and
//    returns false (with dest empty) if src contains invalid chars; for
//    this version src and dest must be different strings.
// ----------------------------------------------------------------------
int WebSafeBase64Unescape(const char* src, int slen, char* dest, int szdest);
bool WebSafeBase64Unescape(absl::string_view src, std::string* dest);

// Return the length to use for the output buffer given to the base64 escape
// routines. Make sure to use the same value for do_padding in both.
// This function may return incorrect results if given input_len values that
// are extremely high, which should happen rarely.
int CalculateBase64EscapedLen(int input_len, bool do_padding);
// Use this version when calling Base64Escape without a do_padding arg.
int CalculateBase64EscapedLen(int input_len);

// ----------------------------------------------------------------------
// Base64Escape()
// WebSafeBase64Escape()
//    Encode "src" to "dest" using base64 encoding.
//    src is not null terminated, instead specify len.
//    'dest' should have at least CalculateBase64EscapedLen() length.
//    RETURNS the length of dest.
//    The WebSafe variation use '-' instead of '+' and '_' instead of '/'
//    so that we can place the out in the URL or cookies without having
//    to escape them.  It also has an extra parameter "do_padding",
//    which when set to false will prevent padding with "=".
// ----------------------------------------------------------------------
int Base64Escape(const unsigned char* src, int slen, char* dest, int szdest);
int WebSafeBase64Escape(const unsigned char* src, int slen, char* dest,
                        int szdest, bool do_padding);
// Encode src into dest with padding.
void Base64Escape(absl::string_view src, std::string* dest);
// Encode src into dest web-safely without padding.
void WebSafeBase64Escape(absl::string_view src, std::string* dest);
// Encode src into dest web-safely with padding.
void WebSafeBase64EscapeWithPadding(absl::string_view src, std::string* dest);

void Base64Escape(const unsigned char* src, int szsrc, std::string* dest,
                  bool do_padding);
void WebSafeBase64Escape(const unsigned char* src, int szsrc, std::string* dest,
                         bool do_padding);

inline bool IsValidCodePoint(uint32_t code_point) {
  return code_point < 0xD800 ||
         (code_point >= 0xE000 && code_point <= 0x10FFFF);
}

static const int UTFmax = 4;
// ----------------------------------------------------------------------
// EncodeAsUTF8Char()
//  Helper to append a Unicode code point to a string as UTF8, without bringing
//  in any external dependencies. The output buffer must be as least 4 bytes
//  large.
// ----------------------------------------------------------------------
int EncodeAsUTF8Char(uint32_t code_point, char* output);

// ----------------------------------------------------------------------
// UTF8FirstLetterNumBytes()
//   Length of the first UTF-8 character.
// ----------------------------------------------------------------------
int UTF8FirstLetterNumBytes(const char* src, int len);

// From google3/third_party/absl/strings/escaping.h

// ----------------------------------------------------------------------
// CleanStringLineEndings()
//   Clean up a multi-line string to conform to Unix line endings.
//   Reads from src and appends to dst, so usually dst should be empty.
//
//   If there is no line ending at the end of a non-empty string, it can
//   be added automatically.
//
//   Four different types of input are correctly handled:
//
//     - Unix/Linux files: line ending is LF: pass through unchanged
//
//     - DOS/Windows files: line ending is CRLF: convert to LF
//
//     - Legacy Mac files: line ending is CR: convert to LF
//
//     - Garbled files: random line endings: convert gracefully
//                      lonely CR, lonely LF, CRLF: convert to LF
//
//   @param src The multi-line string to convert
//   @param dst The converted string is appended to this string
//   @param auto_end_last_line Automatically terminate the last line
//
//   Limitations:
//
//     This does not do the right thing for CRCRLF files created by
//     broken programs that do another Unix->DOS conversion on files
//     that are already in CRLF format.  For this, a two-pass approach
//     brute-force would be needed that
//
//       (1) determines the presence of LF (first one is ok)
//       (2) if yes, removes any CR, else convert every CR to LF
void CleanStringLineEndings(const std::string& src, std::string* dst,
                            bool auto_end_last_line);

// Same as above, but transforms the argument in place.
void CleanStringLineEndings(std::string* str, bool auto_end_last_line);

namespace strings {
inline bool EndsWith(absl::string_view text, absl::string_view suffix) {
  return suffix.empty() || (text.size() >= suffix.size() &&
                            memcmp(text.data() + (text.size() - suffix.size()),
                                   suffix.data(), suffix.size()) == 0);
}
}  // namespace strings

namespace internal {

// A locale-independent version of the standard strtod(), which always
// uses a dot as the decimal separator.
double NoLocaleStrtod(const char* str, char** endptr);

}  // namespace internal

}  // namespace protobuf
}  // namespace google


#endif  // GOOGLE_PROTOBUF_STUBS_STRUTIL_H__
