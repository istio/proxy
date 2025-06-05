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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_STRINGS_JSON_ESCAPING_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_STRINGS_JSON_ESCAPING_H_

#include <cstdint>

#include "google/protobuf/stubs/bytestream.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class JsonEscaping {
 public:
  // The minimum value of a unicode high-surrogate code unit in the utf-16
  // encoding. A high-surrogate is also known as a leading-surrogate.
  // See http://www.unicode.org/glossary/#high_surrogate_code_unit
  static constexpr uint16_t kMinHighSurrogate = 0xd800;

  // The maximum value of a unicide high-surrogate code unit in the utf-16
  // encoding. A high-surrogate is also known as a leading-surrogate.
  // See http://www.unicode.org/glossary/#high_surrogate_code_unit
  static constexpr uint16_t kMaxHighSurrogate = 0xdbff;

  // The minimum value of a unicode low-surrogate code unit in the utf-16
  // encoding. A low-surrogate is also known as a trailing-surrogate.
  // See http://www.unicode.org/glossary/#low_surrogate_code_unit
  static constexpr uint16_t kMinLowSurrogate = 0xdc00;

  // The maximum value of a unicode low-surrogate code unit in the utf-16
  // encoding. A low-surrogate is also known as a trailing surrogate.
  // See http://www.unicode.org/glossary/#low_surrogate_code_unit
  static constexpr uint16_t kMaxLowSurrogate = 0xdfff;

  // The minimum value of a unicode supplementary code point.
  // See http://www.unicode.org/glossary/#supplementary_code_point
  static constexpr uint32_t kMinSupplementaryCodePoint = 0x010000;

  // The minimum value of a unicode code point.
  // See http://www.unicode.org/glossary/#code_point
  static constexpr uint32_t kMinCodePoint = 0x000000;

  // The maximum value of a unicode code point.
  // See http://www.unicode.org/glossary/#code_point
  static constexpr uint32_t kMaxCodePoint = 0x10ffff;

  JsonEscaping() {}
  JsonEscaping(const JsonEscaping&) = delete;
  JsonEscaping& operator=(const JsonEscaping&) = delete;
  virtual ~JsonEscaping() {}

  // Escape the given ByteSource to the given ByteSink.
  static void Escape(strings::ByteSource* input, strings::ByteSink* output);

  // Escape the given ByteSource to the given ByteSink.
  // This is optimized for the case where the string is all printable 7-bit
  // ASCII and does not contain a few other characters (such as quotes).
  static void Escape(absl::string_view input, strings::ByteSink* output);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_STRINGS_JSON_ESCAPING_H_
