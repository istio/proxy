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

#include "internal/utf8.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "internal/unicode.h"

// Implementation is based on
// https://go.googlesource.com/go/+/refs/heads/master/src/unicode/utf8/utf8.go
// but adapted for C++.

namespace cel::internal {

namespace {

constexpr uint8_t kUtf8RuneSelf = 0x80;
constexpr size_t kUtf8Max = 4;

constexpr uint8_t kLow = 0x80;
constexpr uint8_t kHigh = 0xbf;

constexpr uint8_t kMaskX = 0x3f;
constexpr uint8_t kMask2 = 0x1f;
constexpr uint8_t kMask3 = 0xf;
constexpr uint8_t kMask4 = 0x7;

constexpr uint8_t kTX = 0x80;
constexpr uint8_t kT2 = 0xc0;
constexpr uint8_t kT3 = 0xe0;
constexpr uint8_t kT4 = 0xf0;

constexpr uint8_t kXX = 0xf1;
constexpr uint8_t kAS = 0xf0;
constexpr uint8_t kS1 = 0x02;
constexpr uint8_t kS2 = 0x13;
constexpr uint8_t kS3 = 0x03;
constexpr uint8_t kS4 = 0x23;
constexpr uint8_t kS5 = 0x34;
constexpr uint8_t kS6 = 0x04;
constexpr uint8_t kS7 = 0x44;

// NOLINTBEGIN
// clang-format off
constexpr uint8_t kLeading[256] = {
  //   1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x00-0x0F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x10-0x1F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x20-0x2F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x30-0x3F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x40-0x4F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x50-0x5F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x60-0x6F
  kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, kAS, // 0x70-0x7F
  //   1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
  kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, // 0x80-0x8F
  kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, // 0x90-0x9F
  kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, // 0xA0-0xAF
  kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, // 0xB0-0xBF
  kXX, kXX, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, // 0xC0-0xCF
  kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, kS1, // 0xD0-0xDF
  kS2, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS3, kS4, kS3, kS3, // 0xE0-0xEF
  kS5, kS6, kS6, kS6, kS7, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, kXX, // 0xF0-0xFF
};
// clang-format on
// NOLINTEND

constexpr std::pair<const uint8_t, const uint8_t> kAccept[16] = {
    {kLow, kHigh}, {0xa0, kHigh}, {kLow, 0x9f}, {0x90, kHigh},
    {kLow, 0x8f},  {0x0, 0x0},    {0x0, 0x0},   {0x0, 0x0},
    {0x0, 0x0},    {0x0, 0x0},    {0x0, 0x0},   {0x0, 0x0},
    {0x0, 0x0},    {0x0, 0x0},    {0x0, 0x0},   {0x0, 0x0},
};

class StringReader final {
 public:
  constexpr explicit StringReader(absl::string_view input) : input_(input) {}

  size_t Remaining() const { return input_.size(); }

  bool HasRemaining() const { return !input_.empty(); }

  absl::string_view Peek(size_t n) {
    ABSL_ASSERT(n <= Remaining());
    return input_.substr(0, n);
  }

  char Read() {
    ABSL_ASSERT(HasRemaining());
    char value = input_.front();
    input_.remove_prefix(1);
    return value;
  }

  void Advance(size_t n) {
    ABSL_ASSERT(n <= Remaining());
    input_.remove_prefix(n);
  }

  void Reset(absl::string_view input) { input_ = input; }

 private:
  absl::string_view input_;
};

class CordReader final {
 public:
  explicit CordReader(const absl::Cord& input)
      : input_(input), size_(input_.size()), buffer_(), index_(0) {}

  size_t Remaining() const { return size_; }

  bool HasRemaining() const { return size_ != 0; }

  absl::string_view Peek(size_t n) {
    ABSL_ASSERT(n <= Remaining());
    if (n == 0) {
      return absl::string_view();
    }
    if (n <= buffer_.size() - index_) {
      // Enough data remaining in temporary buffer.
      return absl::string_view(buffer_.data() + index_, n);
    }
    // We do not have enough data. See if we can fit it without allocating by
    // shifting data back to the beginning of the buffer.
    if (buffer_.capacity() >= n) {
      // It will fit in the current capacity, see if we need to shift the
      // existing data to make it fit.
      if (buffer_.capacity() - buffer_.size() < n && index_ != 0) {
        // We need to shift.
        buffer_.erase(buffer_.begin(), buffer_.begin() + index_);
        index_ = 0;
      }
    }
    // Ensure we never reserve less than kUtf8Max.
    buffer_.reserve(std::max(buffer_.size() + n, kUtf8Max));
    size_t to_copy = n - (buffer_.size() - index_);
    absl::CopyCordToString(input_.Subcord(0, to_copy), &buffer_);
    input_.RemovePrefix(to_copy);
    return absl::string_view(buffer_.data() + index_, n);
  }

  char Read() {
    char value = Peek(1).front();
    Advance(1);
    return value;
  }

  void Advance(size_t n) {
    ABSL_ASSERT(n <= Remaining());
    if (n == 0) {
      return;
    }
    if (index_ < buffer_.size()) {
      size_t count = std::min(n, buffer_.size() - index_);
      index_ += count;
      n -= count;
      size_ -= count;
      if (index_ < buffer_.size()) {
        return;
      }
      // Temporary buffer is empty, clear it.
      buffer_.clear();
      index_ = 0;
    }
    input_.RemovePrefix(n);
    size_ -= n;
  }

  void Reset(const absl::Cord& input) {
    input_ = input;
    size_ = input_.size();
    buffer_.clear();
    index_ = 0;
  }

 private:
  absl::Cord input_;
  size_t size_;
  std::string buffer_;
  size_t index_;
};

template <typename BufferedByteReader>
bool Utf8IsValidImpl(BufferedByteReader* reader) {
  while (reader->HasRemaining()) {
    const auto b = static_cast<uint8_t>(reader->Read());
    if (b < kUtf8RuneSelf) {
      continue;
    }
    const auto leading = kLeading[b];
    if (leading == kXX) {
      return false;
    }
    const auto size = static_cast<size_t>(leading & 7) - 1;
    if (size > reader->Remaining()) {
      return false;
    }
    const absl::string_view segment = reader->Peek(size);
    const auto& accept = kAccept[leading >> 4];
    if (static_cast<uint8_t>(segment[0]) < accept.first ||
        static_cast<uint8_t>(segment[0]) > accept.second) {
      return false;
    } else if (size == 1) {
    } else if (static_cast<uint8_t>(segment[1]) < kLow ||
               static_cast<uint8_t>(segment[1]) > kHigh) {
      return false;
    } else if (size == 2) {
    } else if (static_cast<uint8_t>(segment[2]) < kLow ||
               static_cast<uint8_t>(segment[2]) > kHigh) {
      return false;
    }
    reader->Advance(size);
  }
  return true;
}

template <typename BufferedByteReader>
size_t Utf8CodePointCountImpl(BufferedByteReader* reader) {
  size_t count = 0;
  while (reader->HasRemaining()) {
    count++;
    const auto b = static_cast<uint8_t>(reader->Read());
    if (b < kUtf8RuneSelf) {
      continue;
    }
    const auto leading = kLeading[b];
    if (leading == kXX) {
      continue;
    }
    auto size = static_cast<size_t>(leading & 7) - 1;
    if (size > reader->Remaining()) {
      continue;
    }
    const absl::string_view segment = reader->Peek(size);
    const auto& accept = kAccept[leading >> 4];
    if (static_cast<uint8_t>(segment[0]) < accept.first ||
        static_cast<uint8_t>(segment[0]) > accept.second) {
      size = 0;
    } else if (size == 1) {
    } else if (static_cast<uint8_t>(segment[1]) < kLow ||
               static_cast<uint8_t>(segment[1]) > kHigh) {
      size = 0;
    } else if (size == 2) {
    } else if (static_cast<uint8_t>(segment[2]) < kLow ||
               static_cast<uint8_t>(segment[2]) > kHigh) {
      size = 0;
    }
    reader->Advance(size);
  }
  return count;
}

template <typename BufferedByteReader>
std::pair<size_t, bool> Utf8ValidateImpl(BufferedByteReader* reader) {
  size_t count = 0;
  while (reader->HasRemaining()) {
    const auto b = static_cast<uint8_t>(reader->Read());
    if (b < kUtf8RuneSelf) {
      count++;
      continue;
    }
    const auto leading = kLeading[b];
    if (leading == kXX) {
      return {count, false};
    }
    const auto size = static_cast<size_t>(leading & 7) - 1;
    if (size > reader->Remaining()) {
      return {count, false};
    }
    const absl::string_view segment = reader->Peek(size);
    const auto& accept = kAccept[leading >> 4];
    if (static_cast<uint8_t>(segment[0]) < accept.first ||
        static_cast<uint8_t>(segment[0]) > accept.second) {
      return {count, false};
    } else if (size == 1) {
      count++;
    } else if (static_cast<uint8_t>(segment[1]) < kLow ||
               static_cast<uint8_t>(segment[1]) > kHigh) {
      return {count, false};
    } else if (size == 2) {
      count++;
    } else if (static_cast<uint8_t>(segment[2]) < kLow ||
               static_cast<uint8_t>(segment[2]) > kHigh) {
      return {count, false};
    } else {
      count++;
    }
    reader->Advance(size);
  }
  return {count, true};
}

}  // namespace

bool Utf8IsValid(absl::string_view str) {
  StringReader reader(str);
  bool valid = Utf8IsValidImpl(&reader);
  ABSL_ASSERT((reader.Reset(str), valid == Utf8ValidateImpl(&reader).second));
  return valid;
}

bool Utf8IsValid(const absl::Cord& str) {
  CordReader reader(str);
  bool valid = Utf8IsValidImpl(&reader);
  ABSL_ASSERT((reader.Reset(str), valid == Utf8ValidateImpl(&reader).second));
  return valid;
}

size_t Utf8CodePointCount(absl::string_view str) {
  StringReader reader(str);
  return Utf8CodePointCountImpl(&reader);
}

size_t Utf8CodePointCount(const absl::Cord& str) {
  CordReader reader(str);
  return Utf8CodePointCountImpl(&reader);
}

std::pair<size_t, bool> Utf8Validate(absl::string_view str) {
  StringReader reader(str);
  auto result = Utf8ValidateImpl(&reader);
  ABSL_ASSERT((reader.Reset(str), result.second == Utf8IsValidImpl(&reader)));
  return result;
}

std::pair<size_t, bool> Utf8Validate(const absl::Cord& str) {
  CordReader reader(str);
  auto result = Utf8ValidateImpl(&reader);
  ABSL_ASSERT((reader.Reset(str), result.second == Utf8IsValidImpl(&reader)));
  return result;
}

namespace {

size_t Utf8DecodeImpl(uint8_t b, uint8_t leading, size_t size,
                      absl::string_view str,
                      char32_t* absl_nullable code_point) {
  const auto& accept = kAccept[leading >> 4];
  const auto b1 = static_cast<uint8_t>(str.front());
  if (ABSL_PREDICT_FALSE(b1 < accept.first || b1 > accept.second)) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  if (size <= 1) {
    if (code_point != nullptr) {
      *code_point = (static_cast<char32_t>(b & kMask2) << 6) |
                    static_cast<char32_t>(b1 & kMaskX);
    }
    return 2;
  }
  str.remove_prefix(1);
  const auto b2 = static_cast<uint8_t>(str.front());
  if (ABSL_PREDICT_FALSE(b2 < kLow || b2 > kHigh)) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  if (size <= 2) {
    if (code_point != nullptr) {
      *code_point = (static_cast<char32_t>(b & kMask3) << 12) |
                    (static_cast<char32_t>(b1 & kMaskX) << 6) |
                    static_cast<char32_t>(b2 & kMaskX);
    }
    return 3;
  }
  str.remove_prefix(1);
  const auto b3 = static_cast<uint8_t>(str.front());
  if (ABSL_PREDICT_FALSE(b3 < kLow || b3 > kHigh)) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  if (code_point != nullptr) {
    *code_point = (static_cast<char32_t>(b & kMask4) << 18) |
                  (static_cast<char32_t>(b1 & kMaskX) << 12) |
                  (static_cast<char32_t>(b2 & kMaskX) << 6) |
                  static_cast<char32_t>(b3 & kMaskX);
  }
  return 4;
}

}  // namespace

size_t Utf8Decode(absl::string_view str, char32_t* absl_nullable code_point) {
  ABSL_DCHECK(!str.empty());
  const auto b = static_cast<uint8_t>(str.front());
  if (b < kUtf8RuneSelf) {
    if (code_point != nullptr) {
      *code_point = static_cast<char32_t>(b);
    }
    return 1;
  }
  const auto leading = kLeading[b];
  if (ABSL_PREDICT_FALSE(leading == kXX)) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  auto size = static_cast<size_t>(leading & 7) - 1;
  str.remove_prefix(1);
  if (ABSL_PREDICT_FALSE(size > str.size())) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  return Utf8DecodeImpl(b, leading, size, str, code_point);
}

size_t Utf8Decode(const absl::Cord::CharIterator& it,
                  char32_t* absl_nullable code_point) {
  absl::string_view str = absl::Cord::ChunkRemaining(it);
  ABSL_DCHECK(!str.empty());
  const auto b = static_cast<uint8_t>(str.front());
  if (b < kUtf8RuneSelf) {
    if (code_point != nullptr) {
      *code_point = static_cast<char32_t>(b);
    }
    return 1;
  }
  const auto leading = kLeading[b];
  if (ABSL_PREDICT_FALSE(leading == kXX)) {
    if (code_point != nullptr) {
      *code_point = kUnicodeReplacementCharacter;
    }
    return 1;
  }
  auto size = static_cast<size_t>(leading & 7) - 1;
  str.remove_prefix(1);
  if (ABSL_PREDICT_TRUE(size <= str.size())) {
    // Fast path.
    return Utf8DecodeImpl(b, leading, size, str, code_point);
  }
  absl::Cord::CharIterator current = it;
  absl::Cord::Advance(&current, 1);
  char buffer[3];
  size_t buffer_len = 0;
  while (buffer_len < size) {
    str = absl::Cord::ChunkRemaining(current);
    if (ABSL_PREDICT_FALSE(str.empty())) {
      if (code_point != nullptr) {
        *code_point = kUnicodeReplacementCharacter;
      }
      return 1;
    }
    size_t to_copy = std::min(size_t{3} - buffer_len, str.size());
    std::memcpy(buffer + buffer_len, str.data(), to_copy);
    buffer_len += to_copy;
    absl::Cord::Advance(&current, to_copy);
  }
  return Utf8DecodeImpl(b, leading, size, absl::string_view(buffer, buffer_len),
                        code_point);
}

size_t Utf8Encode(char32_t code_point, std::string* absl_nonnull buffer) {
  ABSL_DCHECK(buffer != nullptr);

  char storage[4];
  size_t storage_len = Utf8Encode(code_point, storage);
  buffer->append(storage, storage_len);
  return storage_len;
}

size_t Utf8Encode(char32_t code_point, char* absl_nonnull buffer) {
  ABSL_DCHECK(buffer != nullptr);

  if (ABSL_PREDICT_FALSE(!UnicodeIsValid(code_point))) {
    code_point = kUnicodeReplacementCharacter;
  }
  size_t storage_len = 0;
  if (code_point <= 0x7f) {
    buffer[storage_len++] = static_cast<char>(static_cast<uint8_t>(code_point));
  } else if (code_point <= 0x7ff) {
    buffer[storage_len++] =
        static_cast<char>(kT2 | static_cast<uint8_t>(code_point >> 6));
    buffer[storage_len++] =
        static_cast<char>(kTX | (static_cast<uint8_t>(code_point) & kMaskX));
  } else if (code_point <= 0xffff) {
    buffer[storage_len++] =
        static_cast<char>(kT3 | static_cast<uint8_t>(code_point >> 12));
    buffer[storage_len++] = static_cast<char>(
        kTX | (static_cast<uint8_t>(code_point >> 6) & kMaskX));
    buffer[storage_len++] =
        static_cast<char>(kTX | (static_cast<uint8_t>(code_point) & kMaskX));
  } else {
    buffer[storage_len++] =
        static_cast<char>(kT4 | static_cast<uint8_t>(code_point >> 18));
    buffer[storage_len++] = static_cast<char>(
        kTX | (static_cast<uint8_t>(code_point >> 12) & kMaskX));
    buffer[storage_len++] = static_cast<char>(
        kTX | (static_cast<uint8_t>(code_point >> 6) & kMaskX));
    buffer[storage_len++] =
        static_cast<char>(kTX | (static_cast<uint8_t>(code_point) & kMaskX));
  }
  return storage_len;
}

}  // namespace cel::internal
