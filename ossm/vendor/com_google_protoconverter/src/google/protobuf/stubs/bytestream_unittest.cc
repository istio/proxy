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

#include "google/protobuf/stubs/bytestream.h"

#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "google/protobuf/testing/googletest.h"
namespace google {
namespace protobuf {
namespace strings {
namespace {
// We use this class instead of ArrayByteSource to simulate a ByteSource that
// contains multiple fragments.  ArrayByteSource returns the entire array in
// one fragment.
class MockByteSource : public ByteSource {
 public:
  MockByteSource(absl::string_view data, int block_size)
      : data_(data), block_size_(block_size) {}
  size_t Available() const { return data_.size(); }
  absl::string_view Peek() { return data_.substr(0, block_size_); }
  void Skip(size_t n) { data_.remove_prefix(n); }

 private:
  absl::string_view data_;
  int block_size_;
};
TEST(ByteSourceTest, CopyTo) {
  absl::string_view data("Hello world!");
  MockByteSource source(data, 3);
  std::string str;
  StringByteSink sink(&str);
  source.CopyTo(&sink, data.size());
  EXPECT_EQ(data, str);
}
TEST(ByteSourceTest, CopySubstringTo) {
  absl::string_view data("Hello world!");
  MockByteSource source(data, 3);
  source.Skip(1);
  std::string str;
  StringByteSink sink(&str);
  source.CopyTo(&sink, data.size() - 2);
  EXPECT_EQ(data.substr(1, data.size() - 2), str);
  EXPECT_EQ("!", source.Peek());
}
TEST(ByteSourceTest, LimitByteSource) {
  absl::string_view data("Hello world!");
  MockByteSource source(data, 3);
  LimitByteSource limit_source(&source, 6);
  EXPECT_EQ(6, limit_source.Available());
  limit_source.Skip(1);
  EXPECT_EQ(5, limit_source.Available());
  {
    std::string str;
    StringByteSink sink(&str);
    limit_source.CopyTo(&sink, limit_source.Available());
    EXPECT_EQ("ello ", str);
    EXPECT_EQ(0, limit_source.Available());
    EXPECT_EQ(6, source.Available());
  }
  {
    std::string str;
    StringByteSink sink(&str);
    source.CopyTo(&sink, source.Available());
    EXPECT_EQ("world!", str);
    EXPECT_EQ(0, source.Available());
  }
}
TEST(ByteSourceTest, CopyToStringByteSink) {
  absl::string_view data("Hello world!");
  MockByteSource source(data, 3);
  std::string str;
  StringByteSink sink(&str);
  source.CopyTo(&sink, data.size());
  EXPECT_EQ(data, str);
}
// Verify that ByteSink is subclassable and Flush() overridable.
class FlushingByteSink : public StringByteSink {
 public:
  explicit FlushingByteSink(std::string* dest) : StringByteSink(dest) {}
  virtual void Flush() { Append("z", 1); }
};
// Write and Flush via the ByteSink superclass interface.
void WriteAndFlush(ByteSink* s) {
  s->Append("abc", 3);
  s->Flush();
}
TEST(ByteSinkTest, Flush) {
  std::string str;
  FlushingByteSink f_sink(&str);
  WriteAndFlush(&f_sink);
  EXPECT_STREQ("abcz", str.c_str());
}
}  // namespace
}  // namespace strings
}  // namespace protobuf
}  // namespace google
