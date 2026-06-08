// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/json_escaping.h"

#include <stdio.h>

#include <cstdint>

#include <gtest/gtest.h>
#include "absl/log/absl_log.h"
#include "absl/strings/string_view.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

static const char NULL_CHAR = '\0';

class JsonEscapingTest : public ::testing::Test {
 protected:
  JsonEscapingTest() {}
  ~JsonEscapingTest() override {}

  // SplitStringByteSource is similar to strings::LimitByteSource
  // except that we have a vector of limits instead of just one limit,
  // and when a limit a reached, it continues with the rest of the string
  // subject to the next limit in the vector.
  // TODO(wpoon): Consider moving this to a testing directory, as it
  //              could possibly be useful to other tests as well.
  class SplitStringByteSource : public strings::ByteSource {
   public:
    SplitStringByteSource(absl::string_view s, std::vector<int> split_points)
        : source_(s), split_points_(split_points), pos_(0), next_split_(0) {
      // Sanity checks to make sure split points are straightly increasing
      // and that they are less than the length of the input string.
      if (!split_points_.empty()) {
        int prev = split_points_[0];
        ABSL_DLOG_IF(FATAL, prev <= 0) << "Split points must be > 0.";
        for (int i = 1; i < split_points_.size(); ++i) {
          ABSL_DLOG_IF(FATAL, prev >= split_points_[i])
              << "Split points should be straightly increasing.";
          ABSL_DLOG_IF(FATAL, split_points_[i] >= s.size())
              << "Split points should not exceed input length.";
          prev = split_points_[i];
        }
      }
    }

    size_t Available() const override {
      size_t available = source_.Available();
      if (next_split_ < split_points_.size() &&
          available > split_points_[next_split_] - pos_) {
        available = split_points_[next_split_];
      }
      return available;
    }

    absl::string_view Peek() override {
      absl::string_view piece(source_.Peek());
      if (next_split_ < split_points_.size() &&
          piece.size() > split_points_[next_split_] - pos_) {
        piece =
            absl::string_view(piece.data(), split_points_[next_split_] - pos_);
      }
      return piece;
    }

    void Skip(size_t n) override {
      if (next_split_ < split_points_.size() &&
          n > split_points_[next_split_] - pos_) {
        ABSL_DLOG(FATAL) << "Cannot skip past split points.";
      }
      source_.Skip(n);
      pos_ += n;
      if (next_split_ < split_points_.size() &&
          pos_ >= split_points_[next_split_]) {
        ++next_split_;
      }
    }

   private:
    strings::ArrayByteSource source_;
    std::vector<int> split_points_;
    int pos_;
    int next_split_;
  };

  // Returns true if cp is among the set of characters that we escape but
  // are not required by the Json spec. Some of them are required for security,
  // e.g. < and > to prevent possible HTML attacks.
  bool IsExtraEscape(uint32_t cp) {
    if ((cp >= 0x7f && cp <= 0x9f) ||              // ASCII control characters
        (cp >= 0x0001d173 && cp <= 0x0001d17a) ||  // Music symbol formatting
        (cp >= 0x000e0020 && cp <= 0x000e007f)) {  // Deprecated TAG symbols
      return true;
    }
    static const uint32_t kExtraEscapes[] = {
        '<',    '>',    0xad,   0x600,  0x601,  0x602,  0x603,     0x6dd,
        0x70f,  0x17b4, 0x17b5, 0x200b, 0x200c, 0x200d, 0x200e,    0x200f,
        0x2028, 0x2029, 0x202a, 0x202b, 0x202c, 0x202d, 0x202e,    0x2060,
        0x2061, 0x2062, 0x2063, 0x2064, 0x206a, 0x206b, 0x206c,    0x206d,
        0x206e, 0x206f, 0xfeff, 0xfff9, 0xfffa, 0xfffb, 0x000e0001};
    for (uint32_t extra : kExtraEscapes) {
      if (cp == extra) return true;
    }
    return false;
  }

  // Converts the specified unicode code point to its UTF-8 representation
  // stored in a char array.
  const char* ToUtf8Chars(uint32_t cp) {
    static char buffer[5];
    if (cp <= 0x7f) {
      buffer[0] = cp;
      buffer[1] = '\0';
    } else if (cp <= 0x7ff) {
      buffer[0] = (cp >> 6) | 0xc0;
      buffer[1] = (cp & 0x3f) | 0x80;
      buffer[2] = '\0';
    } else if (cp <= 0xffff) {
      buffer[0] = (cp >> 12) | 0xe0;
      buffer[1] = ((cp >> 6) & 0x3f) | 0x80;
      buffer[2] = (cp & 0x3f) | 0x80;
      buffer[3] = '\0';
    } else {
      buffer[0] = ((cp >> 18) & 0x07) | 0xf0;
      buffer[1] = ((cp >> 12) & 0x3f) | 0x80;
      buffer[2] = ((cp >> 6) & 0x3f) | 0x80;
      buffer[3] = (cp & 0x3f) | 0x80;
      buffer[4] = '\0';
    }
    return buffer;
  }

  // Converts the specified unicode code point to its UTF-16 representation
  // stored in a char array. If the specified code point is a BMP (Basic
  // Multilingual Plane or Plane 0) value, the resulting char array has the
  // same value as the code point. If the specified code point is a
  // supplementary code point, the resulting char array has the corresponding
  // surrogate pair.
  const uint16_t* ToUtf16Chars(uint32_t cp) {
    static uint16_t buffer[2];
    if ((cp >> 16) == 0) {
      buffer[0] = cp;
    } else {
      buffer[0] = ((cp - JsonEscaping::kMinSupplementaryCodePoint) >> 10) +
                  JsonEscaping::kMinHighSurrogate;
      buffer[1] = (cp & 0x3ff) + JsonEscaping::kMinLowSurrogate;
    }
    return buffer;
  }

  const char* EscapeByteSource(strings::ByteSource* source) {
    static char buffer[100];
    strings::CheckedArrayByteSink sink(buffer, 100);
    JsonEscaping::Escape(source, &sink);
    sink.Append(&NULL_CHAR, 1);
    return buffer;
  }

  const char* EscapeStringFast(absl::string_view sp) {
    static char buffer[100];
    strings::CheckedArrayByteSink sink(buffer, 100);
    JsonEscaping::Escape(sp, &sink);
    sink.Append(&NULL_CHAR, 1);
    return buffer;
  }

  const char* EscapeString(absl::string_view sp) {
    strings::ArrayByteSource source(sp);
    return EscapeByteSource(&source);
  }

  const char* EscapeChar(uint32_t cp) {
    absl::string_view sp(ToUtf8Chars(cp));
    if (cp == 0) sp = absl::string_view(sp.data(), 1);
    return EscapeString(sp);
  }

  const char* EscapeStringWithSplitPoints(absl::string_view sp,
                                          std::vector<int> split_points) {
    SplitStringByteSource source(sp, split_points);
    return EscapeByteSource(&source);
  }
};

TEST_F(JsonEscapingTest, AllValidCodePoints) {
  for (uint32_t cp = JsonEscaping::kMinCodePoint;
       cp <= JsonEscaping::kMaxCodePoint; ++cp) {
    // Skip low and high surrogates. They are not valid by themselves.
    if (cp >= JsonEscaping::kMinHighSurrogate &&
        cp <= JsonEscaping::kMaxLowSurrogate) {
      continue;
    }
    const char* actual = EscapeChar(cp);
    if (cp <= 0x1f || cp == '"' || cp == '\\' || IsExtraEscape(cp)) {
      switch (cp) {
        case '\b': {
          EXPECT_STREQ("\\b", actual);
          break;
        }
        case '\t': {
          EXPECT_STREQ("\\t", actual);
          break;
        }
        case '\n': {
          EXPECT_STREQ("\\n", actual);
          break;
        }
        case '\f': {
          EXPECT_STREQ("\\f", actual);
          break;
        }
        case '\r': {
          EXPECT_STREQ("\\r", actual);
          break;
        }
        case '"': {
          EXPECT_STREQ("\\\"", actual);
          break;
        }
        case '\\': {
          EXPECT_STREQ("\\\\", actual);
          break;
        }
        default: {
          char expected[13];
          if (cp < JsonEscaping::kMinSupplementaryCodePoint) {
            snprintf(expected, sizeof(expected), "\\u%04x", cp);
          } else {
            const uint16_t* array = ToUtf16Chars(cp);
            snprintf(expected, sizeof(expected), "\\u%04x\\u%04x", array[0],
                     array[1]);
          }
          EXPECT_STREQ(expected, actual);
          break;
        }
      }
    } else {
      EXPECT_STREQ(ToUtf8Chars(cp), actual);
    }
  }
}

struct SamplePair {
  const absl::string_view escaped;
  const absl::string_view raw;
};

static const SamplePair kSamplePairs[] = {
    {"\\u0000", absl::string_view("\u0000", 1)},
    {"A\\u0000Z", absl::string_view("A\u0000Z", 3)},
    {"\\u000b", "\u000b"},
    {"\\u001a", "\u001a"},
    {"\\u001f&'", "\u001f&'"},
    {"\\u007f", "\u007f"},
    {"\\\"", "\""},
    {"/", "/"},
    {"\\\\", "\\"},
    {"A\\\\Z", "A\\Z"},
    {"A\\nZ", "A\nZ"},
    {"𝄞", "𝄞"},
    {"\\\"Google\\\" in Chinese is 谷歌 :-)",
     "\"Google\" in Chinese is 谷歌 :-)"}};

TEST_F(JsonEscapingTest, Samples) {
  for (const SamplePair pair : kSamplePairs) {
    EXPECT_STREQ(pair.escaped.data(), EscapeString(pair.raw));
    EXPECT_STREQ(pair.escaped.data(), EscapeStringFast(pair.raw));
  }
}

TEST_F(JsonEscapingTest, SplitInTheMiddleOfUnicode) {
  // Each of the two Chinese characters below takes up 3 bytes in UTF-8.
  // We permutate all possible sets of split points in this range for testing.
  std::vector<int> splits;
  for (int i = 0; i < 0x3f; ++i) {
    splits.clear();
    for (int j = 0; j < 6; ++j) {
      if ((i & (0x01 << j)) > 0) splits.push_back(j + 1);
    }
    EXPECT_STREQ("谷歌 rocks!",
                 EscapeStringWithSplitPoints("谷歌 rocks!", splits));
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
