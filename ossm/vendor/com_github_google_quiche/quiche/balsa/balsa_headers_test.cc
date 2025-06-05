// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that several of the BalsaHeaders functions are
// tested in the balsa_frame_test as the BalsaFrame and
// BalsaHeaders classes are fairly related.

#include "quiche/balsa/balsa_headers.h"

#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "quiche/balsa/balsa_enums.h"
#include "quiche/balsa/balsa_frame.h"
#include "quiche/balsa/simple_buffer.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"

using testing::AnyOf;
using testing::Combine;
using testing::ElementsAre;
using testing::Eq;
using testing::StrEq;
using testing::ValuesIn;

namespace quiche {

namespace test {

class BalsaHeadersTestPeer {
 public:
  static void WriteFromFramer(BalsaHeaders* headers, const char* ptr,
                              size_t size) {
    headers->WriteFromFramer(ptr, size);
  }
};

namespace {

class BalsaBufferTest : public QuicheTest {
 public:
  void CreateBuffer(size_t blocksize) {
    buffer_ = std::make_unique<BalsaBuffer>(blocksize);
  }
  void CreateBuffer() { buffer_ = std::make_unique<BalsaBuffer>(); }
  static std::unique_ptr<BalsaBuffer> CreateUnmanagedBuffer(size_t blocksize) {
    return std::make_unique<BalsaBuffer>(blocksize);
  }
  absl::string_view Write(absl::string_view sp, size_t* block_buffer_idx) {
    if (sp.empty()) {
      return sp;
    }
    char* storage = buffer_->Reserve(sp.size(), block_buffer_idx);
    memcpy(storage, sp.data(), sp.size());
    return absl::string_view(storage, sp.size());
  }

 protected:
  std::unique_ptr<BalsaBuffer> buffer_;
};

using BufferBlock = BalsaBuffer::BufferBlock;

BufferBlock MakeBufferBlock(const std::string& s) {
  // Make the buffer twice the size needed to verify that CopyFrom copies our
  // buffer_size (as opposed to shrinking to fit or reusing an old buffer).
  BufferBlock block{std::make_unique<char[]>(s.size()), s.size() * 2, s.size()};
  std::memcpy(block.buffer.get(), s.data(), s.size());
  return block;
}

BalsaHeaders CreateHTTPHeaders(bool request, absl::string_view s) {
  BalsaHeaders headers;
  BalsaFrame framer;
  framer.set_is_request(request);
  framer.set_balsa_headers(&headers);
  QUICHE_CHECK_EQ(s.size(), framer.ProcessInput(s.data(), s.size()));
  QUICHE_CHECK(framer.MessageFullyRead());
  return headers;
}

class BufferBlockTest
    : public QuicheTestWithParam<std::tuple<const char*, const char*>> {};

TEST_P(BufferBlockTest, CopyFrom) {
  const std::string s1 = std::get<0>(GetParam());
  const std::string s2 = std::get<1>(GetParam());
  BufferBlock block;
  block.CopyFrom(MakeBufferBlock(s1));
  EXPECT_EQ(s1.size(), block.bytes_free);
  ASSERT_EQ(2 * s1.size(), block.buffer_size);
  EXPECT_EQ(0, memcmp(s1.data(), block.buffer.get(), s1.size()));
  block.CopyFrom(MakeBufferBlock(s2));
  EXPECT_EQ(s2.size(), block.bytes_free);
  ASSERT_EQ(2 * s2.size(), block.buffer_size);
  EXPECT_EQ(0, memcmp(s2.data(), block.buffer.get(), s2.size()));
}

const char* block_strings[] = {"short string", "longer than the other string"};
INSTANTIATE_TEST_SUITE_P(VariousSizes, BufferBlockTest,
                         Combine(ValuesIn(block_strings),
                                 ValuesIn(block_strings)));

TEST_F(BalsaBufferTest, BlocksizeSet) {
  CreateBuffer();
  EXPECT_EQ(BalsaBuffer::kDefaultBlocksize, buffer_->blocksize());
  CreateBuffer(1024);
  EXPECT_EQ(1024u, buffer_->blocksize());
}

TEST_F(BalsaBufferTest, GetMemorySize) {
  CreateBuffer(10);
  EXPECT_EQ(0u, buffer_->GetTotalBytesUsed());
  EXPECT_EQ(0u, buffer_->GetTotalBufferBlockSize());
  BalsaBuffer::Blocks::size_type index;
  buffer_->Reserve(1024, &index);
  EXPECT_EQ(10u + 1024u, buffer_->GetTotalBufferBlockSize());
  EXPECT_EQ(1024u, buffer_->GetTotalBytesUsed());
}

TEST_F(BalsaBufferTest, ManyWritesToContiguousBuffer) {
  CreateBuffer(0);
  // The test is that the process completes.  If it needs to do a resize on
  // every write, it will timeout or run out of memory.
  // ( 10 + 20 + 30 + ... + 1.2e6 bytes => ~1e11 bytes )
  std::string data = "0123456789";
  for (int i = 0; i < 120 * 1000; ++i) {
    buffer_->WriteToContiguousBuffer(data);
  }
}

TEST_F(BalsaBufferTest, CopyFrom) {
  CreateBuffer(10);
  std::unique_ptr<BalsaBuffer> ptr = CreateUnmanagedBuffer(1024);
  ASSERT_EQ(1024u, ptr->blocksize());
  EXPECT_EQ(0u, ptr->num_blocks());

  std::string data1 = "foobarbaz01";
  buffer_->WriteToContiguousBuffer(data1);
  buffer_->NoMoreWriteToContiguousBuffer();
  std::string data2 = "12345";
  Write(data2, nullptr);
  std::string data3 = "6789";
  Write(data3, nullptr);
  std::string data4 = "123456789012345";
  Write(data3, nullptr);

  ptr->CopyFrom(*buffer_);

  EXPECT_EQ(ptr->can_write_to_contiguous_buffer(),
            buffer_->can_write_to_contiguous_buffer());
  ASSERT_EQ(ptr->num_blocks(), buffer_->num_blocks());
  for (size_t i = 0; i < buffer_->num_blocks(); ++i) {
    ASSERT_EQ(ptr->bytes_used(i), buffer_->bytes_used(i));
    ASSERT_EQ(ptr->buffer_size(i), buffer_->buffer_size(i));
    EXPECT_EQ(0,
              memcmp(ptr->GetPtr(i), buffer_->GetPtr(i), ptr->bytes_used(i)));
  }
}

TEST_F(BalsaBufferTest, ClearWorks) {
  CreateBuffer(10);

  std::string data1 = "foobarbaz01";
  buffer_->WriteToContiguousBuffer(data1);
  buffer_->NoMoreWriteToContiguousBuffer();
  std::string data2 = "12345";
  Write(data2, nullptr);
  std::string data3 = "6789";
  Write(data3, nullptr);
  std::string data4 = "123456789012345";
  Write(data3, nullptr);

  buffer_->Clear();

  EXPECT_TRUE(buffer_->can_write_to_contiguous_buffer());
  EXPECT_EQ(10u, buffer_->blocksize());
  EXPECT_EQ(0u, buffer_->num_blocks());
}

TEST_F(BalsaBufferTest, ClearWorksWhenLargerThanBlocksize) {
  CreateBuffer(10);

  std::string data1 = "foobarbaz01lkjasdlkjasdlkjasd";
  buffer_->WriteToContiguousBuffer(data1);
  buffer_->NoMoreWriteToContiguousBuffer();
  std::string data2 = "12345";
  Write(data2, nullptr);
  std::string data3 = "6789";
  Write(data3, nullptr);
  std::string data4 = "123456789012345";
  Write(data3, nullptr);

  buffer_->Clear();

  EXPECT_TRUE(buffer_->can_write_to_contiguous_buffer());
  EXPECT_EQ(10u, buffer_->blocksize());
  EXPECT_EQ(0u, buffer_->num_blocks());
}

TEST_F(BalsaBufferTest, ContiguousWriteSmallerThanBlocksize) {
  CreateBuffer(1024);

  std::string data1 = "foo";
  buffer_->WriteToContiguousBuffer(data1);
  std::string composite = data1;
  const char* buf_ptr = buffer_->GetPtr(0);
  ASSERT_LE(composite.size(), buffer_->buffer_size(0));
  EXPECT_EQ(0, memcmp(composite.data(), buf_ptr, composite.size()));

  std::string data2 = "barbaz";
  buffer_->WriteToContiguousBuffer(data2);
  composite += data2;
  buf_ptr = buffer_->GetPtr(0);
  ASSERT_LE(composite.size(), buffer_->buffer_size(0));
  EXPECT_EQ(0, memcmp(composite.data(), buf_ptr, composite.size()));
}

TEST_F(BalsaBufferTest, SingleContiguousWriteLargerThanBlocksize) {
  CreateBuffer(10);

  std::string data1 = "abracadabrawords";
  buffer_->WriteToContiguousBuffer(data1);
  std::string composite = data1;
  const char* buf_ptr = buffer_->GetPtr(0);
  ASSERT_LE(data1.size(), buffer_->buffer_size(0));
  EXPECT_EQ(0, memcmp(composite.data(), buf_ptr, composite.size()))
      << composite << "\n"
      << absl::string_view(buf_ptr, buffer_->bytes_used(0));
}

TEST_F(BalsaBufferTest, ContiguousWriteLargerThanBlocksize) {
  CreateBuffer(10);

  std::string data1 = "123456789";
  buffer_->WriteToContiguousBuffer(data1);
  std::string composite = data1;
  ASSERT_LE(10u, buffer_->buffer_size(0));

  std::string data2 = "0123456789";
  buffer_->WriteToContiguousBuffer(data2);
  composite += data2;

  const char* buf_ptr = buffer_->GetPtr(0);
  ASSERT_LE(composite.size(), buffer_->buffer_size(0));
  EXPECT_EQ(0, memcmp(composite.data(), buf_ptr, composite.size()))
      << "composite: " << composite << "\n"
      << "   actual: " << absl::string_view(buf_ptr, buffer_->bytes_used(0));
}

TEST_F(BalsaBufferTest, TwoContiguousWritesLargerThanBlocksize) {
  CreateBuffer(5);

  std::string data1 = "123456";
  buffer_->WriteToContiguousBuffer(data1);
  std::string composite = data1;
  ASSERT_LE(composite.size(), buffer_->buffer_size(0));

  std::string data2 = "7890123";
  buffer_->WriteToContiguousBuffer(data2);
  composite += data2;

  const char* buf_ptr = buffer_->GetPtr(0);
  ASSERT_LE(composite.size(), buffer_->buffer_size(0));
  EXPECT_EQ(0, memcmp(composite.data(), buf_ptr, composite.size()))
      << "composite: " << composite << "\n"
      << "   actual: " << absl::string_view(buf_ptr, buffer_->bytes_used(0));
}

TEST_F(BalsaBufferTest, WriteSmallerThanBlocksize) {
  CreateBuffer(5);
  std::string data1 = "1234";
  size_t block_idx = 0;
  absl::string_view write_result = Write(data1, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));

  CreateBuffer(5);
  data1 = "1234";
  block_idx = 0;
  write_result = Write(data1, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));
}

TEST_F(BalsaBufferTest, TwoWritesSmallerThanBlocksizeThenAnotherWrite) {
  CreateBuffer(10);
  std::string data1 = "12345";
  size_t block_idx = 0;
  absl::string_view write_result = Write(data1, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));

  std::string data2 = "data2";
  block_idx = 0;
  write_result = Write(data2, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data2));

  std::string data3 = "data3";
  block_idx = 0;
  write_result = Write(data3, &block_idx);
  ASSERT_EQ(2u, block_idx);
  EXPECT_THAT(write_result, StrEq(data3));

  CreateBuffer(10);
  buffer_->NoMoreWriteToContiguousBuffer();
  data1 = "12345";
  block_idx = 0;
  write_result = Write(data1, &block_idx);
  ASSERT_EQ(0u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));

  data2 = "data2";
  block_idx = 0;
  write_result = Write(data2, &block_idx);
  ASSERT_EQ(0u, block_idx);
  EXPECT_THAT(write_result, StrEq(data2));

  data3 = "data3";
  block_idx = 0;
  write_result = Write(data3, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data3));
}

TEST_F(BalsaBufferTest, WriteLargerThanBlocksize) {
  CreateBuffer(5);
  std::string data1 = "123456789";
  size_t block_idx = 0;
  absl::string_view write_result = Write(data1, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));

  CreateBuffer(5);
  buffer_->NoMoreWriteToContiguousBuffer();
  data1 = "123456789";
  block_idx = 0;
  write_result = Write(data1, &block_idx);
  ASSERT_EQ(1u, block_idx);
  EXPECT_THAT(write_result, StrEq(data1));
}

TEST_F(BalsaBufferTest, ContiguousThenTwoSmallerThanBlocksize) {
  CreateBuffer(5);
  std::string data1 = "1234567890";
  buffer_->WriteToContiguousBuffer(data1);
  size_t block_idx = 0;
  std::string data2 = "1234";
  absl::string_view write_result = Write(data2, &block_idx);
  ASSERT_EQ(1u, block_idx);
  std::string data3 = "1234";
  write_result = Write(data3, &block_idx);
  ASSERT_EQ(2u, block_idx);
}

TEST_F(BalsaBufferTest, AccessFirstBlockUninitialized) {
  CreateBuffer(5);
  EXPECT_EQ(0u, buffer_->GetReadableBytesOfFirstBlock());
  EXPECT_QUICHE_BUG(buffer_->StartOfFirstBlock(),
                    "First block not allocated yet!");
  EXPECT_QUICHE_BUG(buffer_->EndOfFirstBlock(),
                    "First block not allocated yet!");
}

TEST_F(BalsaBufferTest, AccessFirstBlockInitialized) {
  CreateBuffer(5);
  std::string data1 = "1234567890";
  buffer_->WriteToContiguousBuffer(data1);
  const char* start = buffer_->StartOfFirstBlock();
  EXPECT_TRUE(start != nullptr);
  const char* end = buffer_->EndOfFirstBlock();
  EXPECT_TRUE(end != nullptr);
  EXPECT_EQ(data1.length(), static_cast<size_t>(end - start));
  EXPECT_EQ(data1.length(), buffer_->GetReadableBytesOfFirstBlock());
}

TEST(BalsaHeaders, CanAssignBeginToIterator) {
  {
    BalsaHeaders header;
    BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
    static_cast<void>(chli);
  }
  {
    const BalsaHeaders header;
    BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
    static_cast<void>(chli);
  }
}

TEST(BalsaHeaders, CanAssignEndToIterator) {
  {
    BalsaHeaders header;
    BalsaHeaders::const_header_lines_iterator chli = header.lines().end();
    static_cast<void>(chli);
  }
  {
    const BalsaHeaders header;
    BalsaHeaders::const_header_lines_iterator chli = header.lines().end();
    static_cast<void>(chli);
  }
}

TEST(BalsaHeaders, ReplaceOrAppendHeaderTestAppending) {
  BalsaHeaders header;
  std::string key_1 = "key_1";
  std::string value_1 = "value_1";
  header.ReplaceOrAppendHeader(key_1, value_1);

  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(absl::string_view("key_1"), chli->first);
  ASSERT_EQ(absl::string_view("value_1"), chli->second);
  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, ReplaceOrAppendHeaderTestReplacing) {
  BalsaHeaders header;
  std::string key_1 = "key_1";
  std::string value_1 = "value_1";
  std::string key_2 = "key_2";
  header.ReplaceOrAppendHeader(key_1, value_1);
  header.ReplaceOrAppendHeader(key_2, value_1);
  std::string value_2 = "value_2_string";
  header.ReplaceOrAppendHeader(key_1, value_2);

  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(key_1, chli->first);
  ASSERT_EQ(value_2, chli->second);
  ++chli;
  ASSERT_EQ(key_2, chli->first);
  ASSERT_EQ(value_1, chli->second);
  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, ReplaceOrAppendHeaderTestReplacingMultiple) {
  BalsaHeaders header;
  std::string key_1 = "key_1";
  std::string key_2 = "key_2";
  std::string value_1 = "val_1";
  std::string value_2 = "val_2";
  std::string value_3 =
      "value_3_is_longer_than_value_1_and_value_2_and_their_keys";
  // Set up header keys 1, 1, 2.  We will replace the value of key 1 with a long
  // enough string that it should be moved to the end.  This regression tests
  // that replacement works if we move the header to the end.
  header.AppendHeader(key_1, value_1);
  header.AppendHeader(key_1, value_2);
  header.AppendHeader(key_2, value_1);
  header.ReplaceOrAppendHeader(key_1, value_3);

  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(key_1, chli->first);
  ASSERT_EQ(value_3, chli->second);
  ++chli;
  ASSERT_EQ(key_2, chli->first);
  ASSERT_EQ(value_1, chli->second);
  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);

  // Now test that replacement works with a shorter value, so that if we ever do
  // in-place replacement it's tested.
  header.ReplaceOrAppendHeader(key_1, value_1);
  chli = header.lines().begin();
  ASSERT_EQ(key_1, chli->first);
  ASSERT_EQ(value_1, chli->second);
  ++chli;
  ASSERT_EQ(key_2, chli->first);
  ASSERT_EQ(value_1, chli->second);
  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, AppendHeaderAndIteratorTest1) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }

  ASSERT_NE(header.lines().begin(), header.lines().end());
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_1"), chli->first);
  ASSERT_EQ(absl::string_view("value_1"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, AppendHeaderAndIteratorTest2) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }
  {
    std::string key_2 = "key_2";
    std::string value_2 = "value_2";
    header.AppendHeader(key_2, value_2);
    key_2 = "garbage";
    value_2 = "garbage";
  }

  ASSERT_NE(header.lines().begin(), header.lines().end());
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_1"), chli->first);
  ASSERT_EQ(absl::string_view("value_1"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_2"), chli->first);
  ASSERT_EQ(absl::string_view("value_2"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, AppendHeaderAndIteratorTest3) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }
  {
    std::string key_2 = "key_2";
    std::string value_2 = "value_2";
    header.AppendHeader(key_2, value_2);
    key_2 = "garbage";
    value_2 = "garbage";
  }
  {
    std::string key_3 = "key_3";
    std::string value_3 = "value_3";
    header.AppendHeader(key_3, value_3);
    key_3 = "garbage";
    value_3 = "garbage";
  }

  ASSERT_NE(header.lines().begin(), header.lines().end());
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ASSERT_EQ(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_1"), chli->first);
  ASSERT_EQ(absl::string_view("value_1"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_2"), chli->first);
  ASSERT_EQ(absl::string_view("value_2"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_3"), chli->first);
  ASSERT_EQ(absl::string_view("value_3"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, AppendHeaderAndTestEraseWithIterator) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }
  {
    std::string key_2 = "key_2";
    std::string value_2 = "value_2";
    header.AppendHeader(key_2, value_2);
    key_2 = "garbage";
    value_2 = "garbage";
  }
  {
    std::string key_3 = "key_3";
    std::string value_3 = "value_3";
    header.AppendHeader(key_3, value_3);
    key_3 = "garbage";
    value_3 = "garbage";
  }
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ++chli;  // should now point to key_2.
  ASSERT_EQ(absl::string_view("key_2"), chli->first);
  header.erase(chli);
  chli = header.lines().begin();

  ASSERT_NE(header.lines().begin(), header.lines().end());
  ASSERT_EQ(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_1"), chli->first);
  ASSERT_EQ(absl::string_view("value_1"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_NE(header.lines().end(), chli);
  ASSERT_EQ(absl::string_view("key_3"), chli->first);
  ASSERT_EQ(absl::string_view("value_3"), chli->second);

  ++chli;
  ASSERT_NE(header.lines().begin(), chli);
  ASSERT_EQ(header.lines().end(), chli);
}

TEST(BalsaHeaders, TestSetFirstlineInAdditionalBuffer) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET / HTTP/1.0"));
}

TEST(BalsaHeaders, TestSetFirstlineInOriginalBufferAndIsShorterThanOriginal) {
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET /foobar HTTP/1.0\r\n"
                                           "\r\n");
  ASSERT_THAT(headers.first_line(), StrEq("GET /foobar HTTP/1.0"));
  // Note that this SetRequestFirstlineFromStringPieces should replace the
  // original one in the -non- 'additional' buffer.
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET / HTTP/1.0"));
}

TEST(BalsaHeaders, TestSetFirstlineInOriginalBufferAndIsLongerThanOriginal) {
  // Similar to above, but this time the new firstline is larger than
  // the original, yet it should still fit into the original -non-
  // 'additional' buffer as the first header-line has been erased.
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "some_key: some_value\r\n"
                                           "another_key: another_value\r\n"
                                           "\r\n");
  ASSERT_THAT(headers.first_line(), StrEq("GET / HTTP/1.0"));
  headers.erase(headers.lines().begin());
  // Note that this SetRequestFirstlineFromStringPieces should replace the
  // original one in the -non- 'additional' buffer.
  headers.SetRequestFirstlineFromStringPieces("GET", "/foobar", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET /foobar HTTP/1.0"));
}

TEST(BalsaHeaders, TestSetFirstlineInAdditionalDataAndIsShorterThanOriginal) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/foobar", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET /foobar HTTP/1.0"));
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET / HTTP/1.0"));
}

TEST(BalsaHeaders, TestSetFirstlineInAdditionalDataAndIsLongerThanOriginal) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET / HTTP/1.0"));
  headers.SetRequestFirstlineFromStringPieces("GET", "/foobar", "HTTP/1.0");
  ASSERT_THAT(headers.first_line(), StrEq("GET /foobar HTTP/1.0"));
}

TEST(BalsaHeaders, TestDeletingSubstring) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key2", "value2");
  headers.AppendHeader("key", "value");
  headers.AppendHeader("unrelated", "value");

  // RemoveAllOfHeader should not delete key1 or key2 given a substring.
  headers.RemoveAllOfHeader("key");
  EXPECT_TRUE(headers.HasHeader("key1"));
  EXPECT_TRUE(headers.HasHeader("key2"));
  EXPECT_TRUE(headers.HasHeader("unrelated"));
  EXPECT_FALSE(headers.HasHeader("key"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("key"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("KeY"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("UNREL"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("key3"));

  EXPECT_FALSE(headers.GetHeader("key1").empty());
  EXPECT_FALSE(headers.GetHeader("KEY1").empty());
  EXPECT_FALSE(headers.GetHeader("key2").empty());
  EXPECT_FALSE(headers.GetHeader("unrelated").empty());
  EXPECT_TRUE(headers.GetHeader("key").empty());

  // Add key back in.
  headers.AppendHeader("key", "");
  EXPECT_TRUE(headers.HasHeader("key"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("key"));
  EXPECT_TRUE(headers.GetHeader("key").empty());

  // RemoveAllHeadersWithPrefix should delete everything starting with key.
  headers.RemoveAllHeadersWithPrefix("key");
  EXPECT_FALSE(headers.HasHeader("key1"));
  EXPECT_FALSE(headers.HasHeader("key2"));
  EXPECT_TRUE(headers.HasHeader("unrelated"));
  EXPECT_FALSE(headers.HasHeader("key"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("key"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("key1"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("key2"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("kEy"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("unrelated"));

  EXPECT_TRUE(headers.GetHeader("key1").empty());
  EXPECT_TRUE(headers.GetHeader("key2").empty());
  EXPECT_FALSE(headers.GetHeader("unrelated").empty());
  EXPECT_TRUE(headers.GetHeader("key").empty());
}

TEST(BalsaHeaders, TestRemovingValues) {
  // Remove entire line from headers, twice. Ensures working line-skipping.
  // Skip consideration of a line whose key is larger than our search key.
  // Skip consideration of a line whose key is smaller than our search key.
  // Skip consideration of a line that is already marked for skipping.
  // Skip consideration of a line whose value is too small.
  // Skip consideration of a line whose key is correct in length but doesn't
  // match.
  {
    BalsaHeaders headers;
    headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
    headers.AppendHeader("hi", "hello");
    headers.AppendHeader("key1", "val1");
    headers.AppendHeader("key1", "value2");
    headers.AppendHeader("key1", "value3");
    headers.AppendHeader("key2", "value4");
    headers.AppendHeader("unrelated", "value");

    EXPECT_EQ(0u, headers.RemoveValue("key1", ""));
    EXPECT_EQ(1u, headers.RemoveValue("key1", "value2"));

    std::string key1_vals = headers.GetAllOfHeaderAsString("key1");
    EXPECT_THAT(key1_vals, StrEq("val1,value3"));

    EXPECT_TRUE(headers.HeaderHasValue("key1", "val1"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "value3"));
    EXPECT_EQ("value4", headers.GetHeader("key2"));
    EXPECT_EQ("hello", headers.GetHeader("hi"));
    EXPECT_EQ("value", headers.GetHeader("unrelated"));
    EXPECT_FALSE(headers.HeaderHasValue("key1", "value2"));

    EXPECT_EQ(1u, headers.RemoveValue("key1", "value3"));

    key1_vals = headers.GetAllOfHeaderAsString("key1");
    EXPECT_THAT(key1_vals, StrEq("val1"));

    EXPECT_TRUE(headers.HeaderHasValue("key1", "val1"));
    EXPECT_EQ("value4", headers.GetHeader("key2"));
    EXPECT_EQ("hello", headers.GetHeader("hi"));
    EXPECT_EQ("value", headers.GetHeader("unrelated"));
    EXPECT_FALSE(headers.HeaderHasValue("key1", "value3"));
    EXPECT_FALSE(headers.HeaderHasValue("key1", "value2"));
  }

  // Remove/keep values with surrounding spaces.
  // Remove values from in between others in multi-value line.
  // Remove entire multi-value line.
  // Keep value in between removed values in multi-value line.
  // Keep trailing value that is too small to be matched after removing a match.
  // Keep value containing matched value (partial but not complete match).
  // Keep an empty header.
  {
    BalsaHeaders headers;
    headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
    headers.AppendHeader("key1", "value1");
    headers.AppendHeader("key1", "value2, value3,value2");
    headers.AppendHeader("key1", "value4 ,value2,value5,val6");
    headers.AppendHeader("key1", "value2,  value2   , value2");
    headers.AppendHeader("key1", "  value2  ,   value2   ");
    headers.AppendHeader("key1", " value2 a");
    headers.AppendHeader("key1", "");
    headers.AppendHeader("key1", ",  ,,");
    headers.AppendHeader("unrelated", "value");

    EXPECT_EQ(8u, headers.RemoveValue("key1", "value2"));

    std::string key1_vals = headers.GetAllOfHeaderAsString("key1");
    EXPECT_THAT(key1_vals,
                StrEq("value1,value3,value4 ,value5,val6,value2 a,,,  ,,"));

    EXPECT_EQ("value", headers.GetHeader("unrelated"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "value1"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "value3"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "value4"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "value5"));
    EXPECT_TRUE(headers.HeaderHasValue("key1", "val6"));
    EXPECT_FALSE(headers.HeaderHasValue("key1", "value2"));
  }

  {
    const absl::string_view key("key");
    const absl::string_view value1("foo\0bar", 7);
    const absl::string_view value2("value2");
    const std::string value = absl::StrCat(value1, ",", value2);

    {
      BalsaHeaders headers;
      headers.AppendHeader(key, value);

      EXPECT_TRUE(headers.HeaderHasValue(key, value1));
      EXPECT_TRUE(headers.HeaderHasValue(key, value2));
      EXPECT_EQ(value, headers.GetAllOfHeaderAsString(key));

      EXPECT_EQ(1u, headers.RemoveValue(key, value2));

      EXPECT_TRUE(headers.HeaderHasValue(key, value1));
      EXPECT_FALSE(headers.HeaderHasValue(key, value2));
      EXPECT_EQ(value1, headers.GetAllOfHeaderAsString(key));
    }

    {
      BalsaHeaders headers;
      headers.AppendHeader(key, value1);
      headers.AppendHeader(key, value2);

      EXPECT_TRUE(headers.HeaderHasValue(key, value1));
      EXPECT_TRUE(headers.HeaderHasValue(key, value2));
      EXPECT_EQ(value, headers.GetAllOfHeaderAsString(key));

      EXPECT_EQ(1u, headers.RemoveValue(key, value2));

      EXPECT_TRUE(headers.HeaderHasValue(key, value1));
      EXPECT_FALSE(headers.HeaderHasValue(key, value2));
      EXPECT_EQ(value1, headers.GetAllOfHeaderAsString(key));
    }
  }
}

TEST(BalsaHeaders, ZeroAppendToHeaderWithCommaAndSpace) {
  // Create an initial header with zero 'X-Forwarded-For' headers.
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "\r\n");

  // Use AppendToHeaderWithCommaAndSpace to add 4 new 'X-Forwarded-For' headers.
  // Appending these headers should preserve the order in which they are added.
  // i.e. 1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "1.1.1.1");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "2.2.2.2");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "3.3.3.3");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "4.4.4.4");

  // Fetch the 'X-Forwarded-For' headers and compare them to the expected order.
  EXPECT_THAT(headers.GetAllOfHeader("X-Forwarded-For"),
              ElementsAre("1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4"));
}

TEST(BalsaHeaders, SingleAppendToHeaderWithCommaAndSpace) {
  // Create an initial header with one 'X-Forwarded-For' header.
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "X-Forwarded-For: 1.1.1.1\r\n"
                                           "\r\n");

  // Use AppendToHeaderWithCommaAndSpace to add 4 new 'X-Forwarded-For' headers.
  // Appending these headers should preserve the order in which they are added.
  // i.e. 1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4, 5.5.5.5
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "2.2.2.2");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "3.3.3.3");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "4.4.4.4");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "5.5.5.5");

  // Fetch the 'X-Forwarded-For' headers and compare them to the expected order.
  EXPECT_THAT(headers.GetAllOfHeader("X-Forwarded-For"),
              ElementsAre("1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4, 5.5.5.5"));
}

TEST(BalsaHeaders, MultipleAppendToHeaderWithCommaAndSpace) {
  // Create an initial header with two 'X-Forwarded-For' headers.
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "X-Forwarded-For: 1.1.1.1\r\n"
                                           "X-Forwarded-For: 2.2.2.2\r\n"
                                           "\r\n");

  // Use AppendToHeaderWithCommaAndSpace to add 4 new 'X-Forwarded-For' headers.
  // Appending these headers should preserve the order in which they are added.
  // i.e. 1.1.1.1, 2.2.2.2, 3.3.3.3, 4.4.4.4, 5.5.5.5, 6.6.6.6
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "3.3.3.3");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "4.4.4.4");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "5.5.5.5");
  headers.AppendToHeaderWithCommaAndSpace("X-Forwarded-For", "6.6.6.6");

  // Fetch the 'X-Forwarded-For' headers and compare them to the expected order.
  EXPECT_THAT(
      headers.GetAllOfHeader("X-Forwarded-For"),
      ElementsAre("1.1.1.1", "2.2.2.2, 3.3.3.3, 4.4.4.4, 5.5.5.5, 6.6.6.6"));
}

TEST(BalsaHeaders, HeaderHasValues) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  // Make sure we find values at the beginning, middle, and end, and we handle
  // multiple .find() calls correctly.
  headers.AppendHeader("key", "val1,val2val2,val2,val3");
  // Make sure we don't mess up comma/boundary checks for beginning, middle and
  // end.
  headers.AppendHeader("key", "val4val5val6");
  headers.AppendHeader("key", "val11 val12");
  headers.AppendHeader("key", "v val13");
  // Make sure we catch the line header
  headers.AppendHeader("key", "val7");
  // Make sure there's no out-of-bounds indexing on an empty line.
  headers.AppendHeader("key", "");
  // Make sure it works when there's spaces before or after a comma.
  headers.AppendHeader("key", "val8 , val9 , val10");
  // Make sure it works when val is surrounded by spaces.
  headers.AppendHeader("key", " val14 ");
  // Make sure other keys aren't used.
  headers.AppendHeader("key2", "val15");
  // Mixed case.
  headers.AppendHeader("key", "Val16");
  headers.AppendHeader("key", "foo, Val17, bar");

  // All case-sensitive.
  EXPECT_TRUE(headers.HeaderHasValue("key", "val1"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val2"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val3"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val7"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val8"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val9"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val10"));
  EXPECT_TRUE(headers.HeaderHasValue("key", "val14"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val4"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val5"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val6"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val11"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val12"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val13"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val15"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val16"));
  EXPECT_FALSE(headers.HeaderHasValue("key", "val17"));

  // All case-insensitive, only change is for val16 and val17.
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val1"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val2"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val3"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val7"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val8"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val9"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val10"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val14"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val4"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val5"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val6"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val11"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val12"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val13"));
  EXPECT_FALSE(headers.HeaderHasValueIgnoreCase("key", "val15"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val16"));
  EXPECT_TRUE(headers.HeaderHasValueIgnoreCase("key", "val17"));
}

// Because we're dealing with one giant buffer, make sure we don't go beyond
// the bounds of the key when doing compares!
TEST(BalsaHeaders, TestNotDeletingBeyondString) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");

  headers.RemoveAllHeadersWithPrefix("key1: value1");
  EXPECT_NE(headers.lines().begin(), headers.lines().end());
}

TEST(BalsaHeaders, TestIteratingOverErasedHeaders) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key2", "value2");
  headers.AppendHeader("key3", "value3");
  headers.AppendHeader("key4", "value4");
  headers.AppendHeader("key5", "value5");
  headers.AppendHeader("key6", "value6");

  headers.RemoveAllOfHeader("key6");
  headers.RemoveAllOfHeader("key5");
  headers.RemoveAllOfHeader("key4");

  BalsaHeaders::const_header_lines_iterator chli = headers.lines().begin();
  EXPECT_NE(headers.lines().end(), chli);
  EXPECT_EQ(headers.lines().begin(), chli);
  EXPECT_THAT(chli->first, StrEq("key1"));
  EXPECT_THAT(chli->second, StrEq("value1"));

  ++chli;
  EXPECT_NE(headers.lines().end(), chli);
  EXPECT_NE(headers.lines().begin(), chli);
  EXPECT_THAT(chli->first, StrEq("key2"));
  EXPECT_THAT(chli->second, StrEq("value2"));

  ++chli;
  EXPECT_NE(headers.lines().end(), chli);
  EXPECT_NE(headers.lines().begin(), chli);
  EXPECT_THAT(chli->first, StrEq("key3"));
  EXPECT_THAT(chli->second, StrEq("value3"));

  ++chli;
  EXPECT_EQ(headers.lines().end(), chli);
  EXPECT_NE(headers.lines().begin(), chli);

  headers.RemoveAllOfHeader("key1");
  headers.RemoveAllOfHeader("key2");
  chli = headers.lines().begin();
  EXPECT_THAT(chli->first, StrEq("key3"));
  EXPECT_THAT(chli->second, StrEq("value3"));
  EXPECT_NE(headers.lines().end(), chli);
  EXPECT_EQ(headers.lines().begin(), chli);

  ++chli;
  EXPECT_EQ(headers.lines().end(), chli);
  EXPECT_NE(headers.lines().begin(), chli);
}

TEST(BalsaHeaders, CanCompareIterators) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }
  {
    std::string key_2 = "key_2";
    std::string value_2 = "value_2";
    header.AppendHeader(key_2, value_2);
    key_2 = "garbage";
    value_2 = "garbage";
  }
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  BalsaHeaders::const_header_lines_iterator chlj = header.lines().begin();
  EXPECT_EQ(chli, chlj);
  ++chlj;
  EXPECT_NE(chli, chlj);
  EXPECT_LT(chli, chlj);
  EXPECT_LE(chli, chlj);
  EXPECT_LE(chli, chli);
  EXPECT_GT(chlj, chli);
  EXPECT_GE(chlj, chli);
  EXPECT_GE(chlj, chlj);
}

TEST(BalsaHeaders, AppendHeaderAndTestThatYouCanEraseEverything) {
  BalsaHeaders header;
  ASSERT_EQ(header.lines().begin(), header.lines().end());
  {
    std::string key_1 = "key_1";
    std::string value_1 = "value_1";
    header.AppendHeader(key_1, value_1);
    key_1 = "garbage";
    value_1 = "garbage";
  }
  {
    std::string key_2 = "key_2";
    std::string value_2 = "value_2";
    header.AppendHeader(key_2, value_2);
    key_2 = "garbage";
    value_2 = "garbage";
  }
  {
    std::string key_3 = "key_3";
    std::string value_3 = "value_3";
    header.AppendHeader(key_3, value_3);
    key_3 = "garbage";
    value_3 = "garbage";
  }
  EXPECT_NE(header.lines().begin(), header.lines().end());
  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  while (chli != header.lines().end()) {
    header.erase(chli);
    chli = header.lines().begin();
  }
  ASSERT_EQ(header.lines().begin(), header.lines().end());
}

TEST(BalsaHeaders, GetHeaderPositionWorksAsExpectedWithNoHeaderLines) {
  BalsaHeaders header;
  BalsaHeaders::const_header_lines_iterator i = header.GetHeaderPosition("foo");
  EXPECT_EQ(i, header.lines().end());
}

TEST(BalsaHeaders, GetHeaderPositionWorksAsExpectedWithBalsaFrameProcessInput) {
  BalsaHeaders headers = CreateHTTPHeaders(
      true,
      "GET / HTTP/1.0\r\n"
      "key1: value_1\r\n"
      "key1: value_foo\r\n"  // this one cannot be fetched via GetHeader
      "key2: value_2\r\n"
      "key3: value_3\r\n"
      "a: value_a\r\n"
      "b: value_b\r\n"
      "\r\n");

  BalsaHeaders::const_header_lines_iterator header_position_b =
      headers.GetHeaderPosition("b");
  ASSERT_NE(header_position_b, headers.lines().end());
  absl::string_view header_key_b_value = header_position_b->second;
  ASSERT_FALSE(header_key_b_value.empty());
  EXPECT_EQ(std::string("value_b"), header_key_b_value);

  BalsaHeaders::const_header_lines_iterator header_position_1 =
      headers.GetHeaderPosition("key1");
  ASSERT_NE(header_position_1, headers.lines().end());
  absl::string_view header_key_1_value = header_position_1->second;
  ASSERT_FALSE(header_key_1_value.empty());
  EXPECT_EQ(std::string("value_1"), header_key_1_value);

  BalsaHeaders::const_header_lines_iterator header_position_3 =
      headers.GetHeaderPosition("key3");
  ASSERT_NE(header_position_3, headers.lines().end());
  absl::string_view header_key_3_value = header_position_3->second;
  ASSERT_FALSE(header_key_3_value.empty());
  EXPECT_EQ(std::string("value_3"), header_key_3_value);

  BalsaHeaders::const_header_lines_iterator header_position_2 =
      headers.GetHeaderPosition("key2");
  ASSERT_NE(header_position_2, headers.lines().end());
  absl::string_view header_key_2_value = header_position_2->second;
  ASSERT_FALSE(header_key_2_value.empty());
  EXPECT_EQ(std::string("value_2"), header_key_2_value);

  BalsaHeaders::const_header_lines_iterator header_position_a =
      headers.GetHeaderPosition("a");
  ASSERT_NE(header_position_a, headers.lines().end());
  absl::string_view header_key_a_value = header_position_a->second;
  ASSERT_FALSE(header_key_a_value.empty());
  EXPECT_EQ(std::string("value_a"), header_key_a_value);
}

TEST(BalsaHeaders, GetHeaderWorksAsExpectedWithNoHeaderLines) {
  BalsaHeaders header;
  absl::string_view value = header.GetHeader("foo");
  EXPECT_TRUE(value.empty());
  value = header.GetHeader("");
  EXPECT_TRUE(value.empty());
}

TEST(BalsaHeaders, HasHeaderWorksAsExpectedWithNoHeaderLines) {
  BalsaHeaders header;
  EXPECT_FALSE(header.HasHeader("foo"));
  EXPECT_FALSE(header.HasHeader(""));
  EXPECT_FALSE(header.HasHeadersWithPrefix("foo"));
  EXPECT_FALSE(header.HasHeadersWithPrefix(""));
}

TEST(BalsaHeaders, HasHeaderWorksAsExpectedWithBalsaFrameProcessInput) {
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "key1: value_1\r\n"
                                           "key1: value_foo\r\n"
                                           "key2:\r\n"
                                           "\r\n");

  EXPECT_FALSE(headers.HasHeader("foo"));
  EXPECT_TRUE(headers.HasHeader("key1"));
  EXPECT_TRUE(headers.HasHeader("key2"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("foo"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("key"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("KEY"));
}

TEST(BalsaHeaders, GetHeaderWorksAsExpectedWithBalsaFrameProcessInput) {
  BalsaHeaders headers = CreateHTTPHeaders(
      true,
      "GET / HTTP/1.0\r\n"
      "key1: value_1\r\n"
      "key1: value_foo\r\n"  // this one cannot be fetched via GetHeader
      "key2: value_2\r\n"
      "key3: value_3\r\n"
      "key4:\r\n"
      "a: value_a\r\n"
      "b: value_b\r\n"
      "\r\n");

  absl::string_view header_key_b_value = headers.GetHeader("b");
  ASSERT_FALSE(header_key_b_value.empty());
  EXPECT_EQ(std::string("value_b"), header_key_b_value);

  absl::string_view header_key_1_value = headers.GetHeader("key1");
  ASSERT_FALSE(header_key_1_value.empty());
  EXPECT_EQ(std::string("value_1"), header_key_1_value);

  absl::string_view header_key_3_value = headers.GetHeader("key3");
  ASSERT_FALSE(header_key_3_value.empty());
  EXPECT_EQ(std::string("value_3"), header_key_3_value);

  absl::string_view header_key_2_value = headers.GetHeader("key2");
  ASSERT_FALSE(header_key_2_value.empty());
  EXPECT_EQ(std::string("value_2"), header_key_2_value);

  absl::string_view header_key_a_value = headers.GetHeader("a");
  ASSERT_FALSE(header_key_a_value.empty());
  EXPECT_EQ(std::string("value_a"), header_key_a_value);

  EXPECT_TRUE(headers.GetHeader("key4").empty());
}

TEST(BalsaHeaders, GetHeaderWorksAsExpectedWithAppendHeader) {
  BalsaHeaders header;

  header.AppendHeader("key1", "value_1");
  // note that this (following) one cannot be found using GetHeader.
  header.AppendHeader("key1", "value_2");
  header.AppendHeader("key2", "value_2");
  header.AppendHeader("key3", "value_3");
  header.AppendHeader("a", "value_a");
  header.AppendHeader("b", "value_b");

  absl::string_view header_key_b_value = header.GetHeader("b");
  absl::string_view header_key_1_value = header.GetHeader("key1");
  absl::string_view header_key_3_value = header.GetHeader("key3");
  absl::string_view header_key_2_value = header.GetHeader("key2");
  absl::string_view header_key_a_value = header.GetHeader("a");

  ASSERT_FALSE(header_key_1_value.empty());
  ASSERT_FALSE(header_key_2_value.empty());
  ASSERT_FALSE(header_key_3_value.empty());
  ASSERT_FALSE(header_key_a_value.empty());
  ASSERT_FALSE(header_key_b_value.empty());

  EXPECT_TRUE(header.HasHeader("key1"));
  EXPECT_TRUE(header.HasHeader("key2"));
  EXPECT_TRUE(header.HasHeader("key3"));
  EXPECT_TRUE(header.HasHeader("a"));
  EXPECT_TRUE(header.HasHeader("b"));

  EXPECT_TRUE(header.HasHeadersWithPrefix("key1"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key2"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key3"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("a"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("b"));

  EXPECT_EQ(std::string("value_1"), header_key_1_value);
  EXPECT_EQ(std::string("value_2"), header_key_2_value);
  EXPECT_EQ(std::string("value_3"), header_key_3_value);
  EXPECT_EQ(std::string("value_a"), header_key_a_value);
  EXPECT_EQ(std::string("value_b"), header_key_b_value);
}

TEST(BalsaHeaders, HasHeaderWorksAsExpectedWithAppendHeader) {
  BalsaHeaders header;

  ASSERT_FALSE(header.HasHeader("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("K"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("ke"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key2"));
  header.AppendHeader("key1", "value_1");
  EXPECT_TRUE(header.HasHeader("key1"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("K"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("ke"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key2"));

  header.AppendHeader("key1", "value_2");
  EXPECT_TRUE(header.HasHeader("key1"));
  EXPECT_FALSE(header.HasHeader("key2"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("k"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("ke"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key2"));
}

TEST(BalsaHeaders, GetHeaderWorksAsExpectedWithHeadersErased) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_1");
  header.AppendHeader("key1", "value_2");
  header.AppendHeader("key2", "value_2");
  header.AppendHeader("key3", "value_3");
  header.AppendHeader("a", "value_a");
  header.AppendHeader("b", "value_b");

  header.erase(header.GetHeaderPosition("key2"));

  absl::string_view header_key_b_value = header.GetHeader("b");
  absl::string_view header_key_1_value = header.GetHeader("key1");
  absl::string_view header_key_3_value = header.GetHeader("key3");
  absl::string_view header_key_2_value = header.GetHeader("key2");
  absl::string_view header_key_a_value = header.GetHeader("a");

  ASSERT_FALSE(header_key_1_value.empty());
  ASSERT_TRUE(header_key_2_value.empty());
  ASSERT_FALSE(header_key_3_value.empty());
  ASSERT_FALSE(header_key_a_value.empty());
  ASSERT_FALSE(header_key_b_value.empty());

  EXPECT_EQ(std::string("value_1"), header_key_1_value);
  EXPECT_EQ(std::string("value_3"), header_key_3_value);
  EXPECT_EQ(std::string("value_a"), header_key_a_value);
  EXPECT_EQ(std::string("value_b"), header_key_b_value);

  // Erasing one makes the next one visible:
  header.erase(header.GetHeaderPosition("key1"));
  header_key_1_value = header.GetHeader("key1");
  ASSERT_FALSE(header_key_1_value.empty());
  EXPECT_EQ(std::string("value_2"), header_key_1_value);

  // Erase both:
  header.erase(header.GetHeaderPosition("key1"));
  ASSERT_TRUE(header.GetHeader("key1").empty());
}

TEST(BalsaHeaders, HasHeaderWorksAsExpectedWithHeadersErased) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_1");
  header.AppendHeader("key2", "value_2a");
  header.AppendHeader("key2", "value_2b");

  ASSERT_TRUE(header.HasHeader("key1"));
  ASSERT_TRUE(header.HasHeadersWithPrefix("key1"));
  ASSERT_TRUE(header.HasHeadersWithPrefix("key2"));
  ASSERT_TRUE(header.HasHeadersWithPrefix("kEY"));
  header.erase(header.GetHeaderPosition("key1"));
  EXPECT_FALSE(header.HasHeader("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key1"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key2"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("kEY"));

  ASSERT_TRUE(header.HasHeader("key2"));
  header.erase(header.GetHeaderPosition("key2"));
  ASSERT_TRUE(header.HasHeader("key2"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key1"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("key2"));
  EXPECT_TRUE(header.HasHeadersWithPrefix("kEY"));
  header.erase(header.GetHeaderPosition("key2"));
  EXPECT_FALSE(header.HasHeader("key2"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key1"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("key2"));
  EXPECT_FALSE(header.HasHeadersWithPrefix("kEY"));
}

TEST(BalsaHeaders, HasNonEmptyHeaderWorksAsExpectedWithNoHeaderLines) {
  BalsaHeaders header;
  EXPECT_FALSE(header.HasNonEmptyHeader("foo"));
  EXPECT_FALSE(header.HasNonEmptyHeader(""));
}

TEST(BalsaHeaders, HasNonEmptyHeaderWorksAsExpectedWithAppendHeader) {
  BalsaHeaders header;

  EXPECT_FALSE(header.HasNonEmptyHeader("key1"));
  header.AppendHeader("key1", "");
  EXPECT_FALSE(header.HasNonEmptyHeader("key1"));

  header.AppendHeader("key1", "value_2");
  EXPECT_TRUE(header.HasNonEmptyHeader("key1"));
  EXPECT_FALSE(header.HasNonEmptyHeader("key2"));
}

TEST(BalsaHeaders, HasNonEmptyHeaderWorksAsExpectedWithHeadersErased) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_1");
  header.AppendHeader("key2", "value_2a");
  header.AppendHeader("key2", "");

  EXPECT_TRUE(header.HasNonEmptyHeader("key1"));
  header.erase(header.GetHeaderPosition("key1"));
  EXPECT_FALSE(header.HasNonEmptyHeader("key1"));

  EXPECT_TRUE(header.HasNonEmptyHeader("key2"));
  header.erase(header.GetHeaderPosition("key2"));
  EXPECT_FALSE(header.HasNonEmptyHeader("key2"));
  header.erase(header.GetHeaderPosition("key2"));
  EXPECT_FALSE(header.HasNonEmptyHeader("key2"));
}

TEST(BalsaHeaders, HasNonEmptyHeaderWorksAsExpectedWithBalsaFrameProcessInput) {
  BalsaHeaders headers = CreateHTTPHeaders(true,
                                           "GET / HTTP/1.0\r\n"
                                           "key1: value_1\r\n"
                                           "key2:\r\n"
                                           "key3:\r\n"
                                           "key3: value_3\r\n"
                                           "key4:\r\n"
                                           "key4:\r\n"
                                           "key5: value_5\r\n"
                                           "key5:\r\n"
                                           "\r\n");

  EXPECT_FALSE(headers.HasNonEmptyHeader("foo"));
  EXPECT_TRUE(headers.HasNonEmptyHeader("key1"));
  EXPECT_FALSE(headers.HasNonEmptyHeader("key2"));
  EXPECT_TRUE(headers.HasNonEmptyHeader("key3"));
  EXPECT_FALSE(headers.HasNonEmptyHeader("key4"));
  EXPECT_TRUE(headers.HasNonEmptyHeader("key5"));

  headers.erase(headers.GetHeaderPosition("key5"));
  EXPECT_FALSE(headers.HasNonEmptyHeader("key5"));
}

TEST(BalsaHeaders, GetAllOfHeader) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("Key", "value_2,value_3");
  header.AppendHeader("key", "");
  header.AppendHeader("KEY", "value_4");

  std::vector<absl::string_view> result;
  header.GetAllOfHeader("key", &result);
  ASSERT_EQ(4u, result.size());
  EXPECT_EQ("value_1", result[0]);
  EXPECT_EQ("value_2,value_3", result[1]);
  EXPECT_EQ("", result[2]);
  EXPECT_EQ("value_4", result[3]);

  EXPECT_EQ(header.GetAllOfHeader("key"), result);
}

TEST(BalsaHeaders, GetAllOfHeaderDoesWhatItSays) {
  BalsaHeaders header;
  // Multiple values for a given header.
  // Some values appear multiple times
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  header.AppendHeader("key", "");
  header.AppendHeader("key", "value_1");

  ASSERT_NE(header.lines().begin(), header.lines().end());
  std::vector<absl::string_view> out;

  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(4u, out.size());
  EXPECT_EQ("value_1", out[0]);
  EXPECT_EQ("value_2", out[1]);
  EXPECT_EQ("", out[2]);
  EXPECT_EQ("value_1", out[3]);

  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderWithPrefix) {
  BalsaHeaders header;
  header.AppendHeader("foo-Foo", "value_1");
  header.AppendHeader("Foo-bar", "value_2,value_3");
  header.AppendHeader("foo-Foo", "");
  header.AppendHeader("bar", "value_not");
  header.AppendHeader("fOO-fOO", "value_4");

  std::vector<std::pair<absl::string_view, absl::string_view>> result;
  header.GetAllOfHeaderWithPrefix("abc", &result);
  ASSERT_EQ(0u, result.size());

  header.GetAllOfHeaderWithPrefix("foo", &result);
  ASSERT_EQ(4u, result.size());
  EXPECT_EQ("foo-Foo", result[0].first);
  EXPECT_EQ("value_1", result[0].second);
  EXPECT_EQ("Foo-bar", result[1].first);
  EXPECT_EQ("value_2,value_3", result[1].second);
  EXPECT_EQ("", result[2].second);
  EXPECT_EQ("value_4", result[3].second);

  std::vector<std::pair<absl::string_view, absl::string_view>> result2;
  header.GetAllOfHeaderWithPrefix("FoO", &result2);
  ASSERT_EQ(4u, result2.size());
}

TEST(BalsaHeaders, GetAllHeadersWithLimit) {
  BalsaHeaders header;
  header.AppendHeader("foo-Foo", "value_1");
  header.AppendHeader("Foo-bar", "value_2,value_3");
  header.AppendHeader("foo-Foo", "");
  header.AppendHeader("bar", "value_4");
  header.AppendHeader("fOO-fOO", "value_5");

  std::vector<std::pair<absl::string_view, absl::string_view>> result;
  header.GetAllHeadersWithLimit(&result, 4);
  ASSERT_EQ(4u, result.size());
  EXPECT_EQ("foo-Foo", result[0].first);
  EXPECT_EQ("value_1", result[0].second);
  EXPECT_EQ("Foo-bar", result[1].first);
  EXPECT_EQ("value_2,value_3", result[1].second);
  EXPECT_EQ("", result[2].second);
  EXPECT_EQ("value_4", result[3].second);

  std::vector<std::pair<absl::string_view, absl::string_view>> result2;
  header.GetAllHeadersWithLimit(&result2, -1);
  ASSERT_EQ(5u, result2.size());
}

TEST(BalsaHeaders, RangeFor) {
  BalsaHeaders header;
  // Multiple values for a given header.
  // Some values appear multiple times
  header.AppendHeader("key1", "value_1a");
  header.AppendHeader("key1", "value_1b");
  header.AppendHeader("key2", "");
  header.AppendHeader("key3", "value_3");

  std::vector<std::pair<absl::string_view, absl::string_view>> out;
  for (const auto& line : header.lines()) {
    out.push_back(line);
  }
  const std::vector<std::pair<absl::string_view, absl::string_view>> expected =
      {{"key1", "value_1a"},
       {"key1", "value_1b"},
       {"key2", ""},
       {"key3", "value_3"}};
  EXPECT_EQ(expected, out);
}

TEST(BalsaHeaders, GetAllOfHeaderWithNonExistentKey) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  std::vector<absl::string_view> out;

  header.GetAllOfHeader("key_non_existent", &out);
  ASSERT_EQ(0u, out.size());

  EXPECT_EQ(header.GetAllOfHeader("key_non_existent"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderEmptyValVariation1) {
  BalsaHeaders header;
  header.AppendHeader("key", "");
  header.AppendHeader("key", "");
  header.AppendHeader("key", "v1");
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("", out[0]);
  EXPECT_EQ("", out[1]);
  EXPECT_EQ("v1", out[2]);

  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderEmptyValVariation2) {
  BalsaHeaders header;
  header.AppendHeader("key", "");
  header.AppendHeader("key", "v1");
  header.AppendHeader("key", "");
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("", out[0]);
  EXPECT_EQ("v1", out[1]);
  EXPECT_EQ("", out[2]);

  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderEmptyValVariation3) {
  BalsaHeaders header;
  header.AppendHeader("key", "");
  header.AppendHeader("key", "v1");
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(2u, out.size());
  EXPECT_EQ("", out[0]);
  EXPECT_EQ("v1", out[1]);

  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderEmptyValVariation4) {
  BalsaHeaders header;
  header.AppendHeader("key", "v1");
  header.AppendHeader("key", "");
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(2u, out.size());
  EXPECT_EQ("v1", out[0]);
  EXPECT_EQ("", out[1]);

  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderWithAppendHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  std::vector<absl::string_view> out;

  header.GetAllOfHeader("key_new", &out);
  ASSERT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("key_new"), out);

  // Add key_new to the header
  header.AppendHeader("key_new", "value_3");
  header.GetAllOfHeader("key_new", &out);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ("value_3", out[0]);
  EXPECT_EQ(header.GetAllOfHeader("key_new"), out);

  // Get the keys that are not modified
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("value_1", out[1]);
  EXPECT_EQ("value_2", out[2]);
  EXPECT_THAT(header.GetAllOfHeader("key"), ElementsAre("value_1", "value_2"));
}

TEST(BalsaHeaders, GetAllOfHeaderWithRemoveHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  header.AppendHeader("a", "va");

  header.RemoveAllOfHeader("key");
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("key"), out);

  header.GetAllOfHeader("a", &out);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("a"), out);

  out.clear();
  header.RemoveAllOfHeader("a");
  header.GetAllOfHeader("a", &out);
  ASSERT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("a"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderWithRemoveNonExistentHeaders) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("Accept-Encoding", "deflate,compress");
  EXPECT_EQ(0u, headers.RemoveValue("Accept-Encoding", "gzip(gfe)"));
  std::string accept_encoding_vals =
      headers.GetAllOfHeaderAsString("Accept-Encoding");
  EXPECT_EQ("deflate,compress", accept_encoding_vals);
}

TEST(BalsaHeaders, GetAllOfHeaderWithEraseHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  header.AppendHeader("a", "va");

  std::vector<absl::string_view> out;

  header.erase(header.GetHeaderPosition("key"));
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ("value_2", out[0]);
  EXPECT_EQ(header.GetAllOfHeader("key"), out);

  out.clear();
  header.erase(header.GetHeaderPosition("key"));
  header.GetAllOfHeader("key", &out);
  ASSERT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("key"), out);

  out.clear();
  header.GetAllOfHeader("a", &out);
  ASSERT_EQ(1u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("a"), out);

  out.clear();
  header.erase(header.GetHeaderPosition("a"));
  header.GetAllOfHeader("a", &out);
  ASSERT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderWithNoHeaderLines) {
  BalsaHeaders header;
  std::vector<absl::string_view> out;
  header.GetAllOfHeader("key", &out);
  EXPECT_EQ(0u, out.size());
  EXPECT_EQ(header.GetAllOfHeader("key"), out);
}

TEST(BalsaHeaders, GetAllOfHeaderDoesWhatItSaysForVariousKeys) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_11");
  header.AppendHeader("key2", "value_21");
  header.AppendHeader("key1", "value_12");
  header.AppendHeader("key2", "value_22");

  std::vector<absl::string_view> out;

  header.GetAllOfHeader("key1", &out);
  EXPECT_EQ("value_11", out[0]);
  EXPECT_EQ("value_12", out[1]);
  EXPECT_EQ(header.GetAllOfHeader("key1"), out);

  header.GetAllOfHeader("key2", &out);
  EXPECT_EQ("value_21", out[2]);
  EXPECT_EQ("value_22", out[3]);
  EXPECT_THAT(header.GetAllOfHeader("key2"),
              ElementsAre("value_21", "value_22"));
}

TEST(BalsaHeaders, GetAllOfHeaderWithBalsaFrameProcessInput) {
  BalsaHeaders header = CreateHTTPHeaders(true,
                                          "GET / HTTP/1.0\r\n"
                                          "key1: value_1\r\n"
                                          "key1: value_foo\r\n"
                                          "key2: value_2\r\n"
                                          "a: value_a\r\n"
                                          "key2: \r\n"
                                          "b: value_b\r\n"
                                          "\r\n");

  std::vector<absl::string_view> out;
  int index = 0;
  header.GetAllOfHeader("key1", &out);
  EXPECT_EQ("value_1", out[index++]);
  EXPECT_EQ("value_foo", out[index++]);
  EXPECT_EQ(header.GetAllOfHeader("key1"), out);

  header.GetAllOfHeader("key2", &out);
  EXPECT_EQ("value_2", out[index++]);
  EXPECT_EQ("", out[index++]);
  EXPECT_THAT(header.GetAllOfHeader("key2"), ElementsAre("value_2", ""));

  header.GetAllOfHeader("a", &out);
  EXPECT_EQ("value_a", out[index++]);
  EXPECT_THAT(header.GetAllOfHeader("a"), ElementsAre("value_a"));

  header.GetAllOfHeader("b", &out);
  EXPECT_EQ("value_b", out[index++]);
  EXPECT_THAT(header.GetAllOfHeader("b"), ElementsAre("value_b"));
}

TEST(BalsaHeaders, GetAllOfHeaderIncludeRemovedDoesWhatItSays) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_11");
  header.AppendHeader("key2", "value_21");
  header.AppendHeader("key1", "value_12");
  header.AppendHeader("key2", "value_22");
  header.AppendHeader("key1", "");

  std::vector<absl::string_view> out;
  header.GetAllOfHeaderIncludeRemoved("key1", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("value_11", out[0]);
  EXPECT_EQ("value_12", out[1]);
  EXPECT_EQ("", out[2]);
  header.GetAllOfHeaderIncludeRemoved("key2", &out);
  ASSERT_EQ(5u, out.size());
  EXPECT_EQ("value_21", out[3]);
  EXPECT_EQ("value_22", out[4]);

  header.erase(header.GetHeaderPosition("key1"));
  out.clear();
  header.GetAllOfHeaderIncludeRemoved("key1", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("value_12", out[0]);
  EXPECT_EQ("", out[1]);
  EXPECT_EQ("value_11", out[2]);
  header.GetAllOfHeaderIncludeRemoved("key2", &out);
  ASSERT_EQ(5u, out.size());
  EXPECT_EQ("value_21", out[3]);
  EXPECT_EQ("value_22", out[4]);

  header.RemoveAllOfHeader("key1");
  out.clear();
  header.GetAllOfHeaderIncludeRemoved("key1", &out);
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ("value_11", out[0]);
  EXPECT_EQ("value_12", out[1]);
  EXPECT_EQ("", out[2]);
  header.GetAllOfHeaderIncludeRemoved("key2", &out);
  ASSERT_EQ(5u, out.size());
  EXPECT_EQ("value_21", out[3]);
  EXPECT_EQ("value_22", out[4]);

  header.Clear();
  out.clear();
  header.GetAllOfHeaderIncludeRemoved("key1", &out);
  ASSERT_EQ(0u, out.size());
  header.GetAllOfHeaderIncludeRemoved("key2", &out);
  ASSERT_EQ(0u, out.size());
}

TEST(BalsaHeaders, GetAllOfHeaderIncludeRemovedWithNonExistentKey) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  std::vector<absl::string_view> out;
  header.GetAllOfHeaderIncludeRemoved("key_non_existent", &out);
  ASSERT_EQ(0u, out.size());
}

TEST(BalsaHeaders, GetIteratorForKeyDoesWhatItSays) {
  BalsaHeaders header;
  // Multiple values for a given header.
  // Some values appear multiple times
  header.AppendHeader("key", "value_1");
  header.AppendHeader("Key", "value_2");
  header.AppendHeader("key", "");
  header.AppendHeader("KEY", "value_1");

  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key");
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("value_1", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("Key", key_it->first);
  EXPECT_EQ("value_2", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("KEY", key_it->first);
  EXPECT_EQ("value_1", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
}

TEST(BalsaHeaders, GetIteratorForKeyWithNonExistentKey) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");

  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key_non_existent");
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
  const auto lines = header.lines("key_non_existent");
  EXPECT_EQ(lines.begin(), header.lines().end());
  EXPECT_EQ(lines.end(), header.header_lines_key_end());
}

TEST(BalsaHeaders, GetIteratorForKeyWithAppendHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");

  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key_new");
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  // Add key_new to the header
  header.AppendHeader("key_new", "value_3");
  key_it = header.GetIteratorForKey("key_new");
  const auto lines1 = header.lines("key_new");
  EXPECT_EQ(lines1.begin(), key_it);
  EXPECT_EQ(lines1.end(), header.header_lines_key_end());

  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key_new", key_it->first);
  EXPECT_EQ("value_3", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  // Get the keys that are not modified
  key_it = header.GetIteratorForKey("key");
  const auto lines2 = header.lines("key");
  EXPECT_EQ(lines2.begin(), key_it);
  EXPECT_EQ(lines2.end(), header.header_lines_key_end());
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("value_1", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("value_2", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
}

TEST(BalsaHeaders, GetIteratorForKeyWithRemoveHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  header.AppendHeader("a", "va");

  header.RemoveAllOfHeader("a");
  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key");
  EXPECT_NE(header.lines().end(), key_it);
  const auto lines1 = header.lines("key");
  EXPECT_EQ(lines1.begin(), key_it);
  EXPECT_EQ(lines1.end(), header.header_lines_key_end());
  EXPECT_EQ("value_1", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("value_2", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  // Check that a typical loop works properly.
  for (BalsaHeaders::const_header_lines_key_iterator it =
           header.GetIteratorForKey("key");
       it != header.lines().end(); ++it) {
    EXPECT_EQ("key", it->first);
  }
}

TEST(BalsaHeaders, GetIteratorForKeyWithEraseHeaders) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  header.AppendHeader("a", "va");
  header.erase(header.GetHeaderPosition("key"));

  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key");
  EXPECT_NE(header.lines().end(), key_it);
  const auto lines1 = header.lines("key");
  EXPECT_EQ(lines1.begin(), key_it);
  EXPECT_EQ(lines1.end(), header.header_lines_key_end());
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key", key_it->first);
  EXPECT_EQ("value_2", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  header.erase(header.GetHeaderPosition("key"));
  key_it = header.GetIteratorForKey("key");
  const auto lines2 = header.lines("key");
  EXPECT_EQ(lines2.begin(), key_it);
  EXPECT_EQ(lines2.end(), header.header_lines_key_end());
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  key_it = header.GetIteratorForKey("a");
  const auto lines3 = header.lines("a");
  EXPECT_EQ(lines3.begin(), key_it);
  EXPECT_EQ(lines3.end(), header.header_lines_key_end());
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("a", key_it->first);
  EXPECT_EQ("va", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  header.erase(header.GetHeaderPosition("a"));
  key_it = header.GetIteratorForKey("a");
  const auto lines4 = header.lines("a");
  EXPECT_EQ(lines4.begin(), key_it);
  EXPECT_EQ(lines4.end(), header.header_lines_key_end());
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
}

TEST(BalsaHeaders, GetIteratorForKeyWithNoHeaderLines) {
  BalsaHeaders header;
  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("key");
  const auto lines = header.lines("key");
  EXPECT_EQ(lines.begin(), key_it);
  EXPECT_EQ(lines.end(), header.header_lines_key_end());
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
}

TEST(BalsaHeaders, GetIteratorForKeyWithBalsaFrameProcessInput) {
  BalsaHeaders header = CreateHTTPHeaders(true,
                                          "GET / HTTP/1.0\r\n"
                                          "key1: value_1\r\n"
                                          "Key1: value_foo\r\n"
                                          "key2: value_2\r\n"
                                          "a: value_a\r\n"
                                          "key2: \r\n"
                                          "b: value_b\r\n"
                                          "\r\n");

  BalsaHeaders::const_header_lines_key_iterator key_it =
      header.GetIteratorForKey("Key1");
  const auto lines1 = header.lines("Key1");
  EXPECT_EQ(lines1.begin(), key_it);
  EXPECT_EQ(lines1.end(), header.header_lines_key_end());
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key1", key_it->first);
  EXPECT_EQ("value_1", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("Key1", key_it->first);
  EXPECT_EQ("value_foo", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  key_it = header.GetIteratorForKey("key2");
  EXPECT_NE(header.lines().end(), key_it);
  const auto lines2 = header.lines("key2");
  EXPECT_EQ(lines2.begin(), key_it);
  EXPECT_EQ(lines2.end(), header.header_lines_key_end());
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key2", key_it->first);
  EXPECT_EQ("value_2", key_it->second);
  ++key_it;
  EXPECT_NE(header.lines().end(), key_it);
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("key2", key_it->first);
  EXPECT_EQ("", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  key_it = header.GetIteratorForKey("a");
  EXPECT_NE(header.lines().end(), key_it);
  const auto lines3 = header.lines("a");
  EXPECT_EQ(lines3.begin(), key_it);
  EXPECT_EQ(lines3.end(), header.header_lines_key_end());
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("a", key_it->first);
  EXPECT_EQ("value_a", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);

  key_it = header.GetIteratorForKey("b");
  EXPECT_NE(header.lines().end(), key_it);
  const auto lines4 = header.lines("b");
  EXPECT_EQ(lines4.begin(), key_it);
  EXPECT_EQ(lines4.end(), header.header_lines_key_end());
  EXPECT_NE(header.header_lines_key_end(), key_it);
  EXPECT_EQ("b", key_it->first);
  EXPECT_EQ("value_b", key_it->second);
  ++key_it;
  EXPECT_EQ(header.lines().end(), key_it);
  EXPECT_EQ(header.header_lines_key_end(), key_it);
}

TEST(BalsaHeaders, GetAllOfHeaderAsStringDoesWhatItSays) {
  BalsaHeaders header;
  // Multiple values for a given header.
  // Some values appear multiple times
  header.AppendHeader("key", "value_1");
  header.AppendHeader("Key", "value_2");
  header.AppendHeader("key", "");
  header.AppendHeader("KEY", "value_1");

  std::string result = header.GetAllOfHeaderAsString("key");
  EXPECT_EQ("value_1,value_2,,value_1", result);
}

TEST(BalsaHeaders, RemoveAllOfHeaderDoesWhatItSays) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  ASSERT_NE(header.lines().begin(), header.lines().end());
  header.RemoveAllOfHeader("key");
  ASSERT_EQ(header.lines().begin(), header.lines().end());
}

TEST(BalsaHeaders,
     RemoveAllOfHeaderDoesWhatItSaysEvenWhenThingsHaveBeenErased) {
  BalsaHeaders header;
  header.AppendHeader("key1", "value_1");
  header.AppendHeader("key1", "value_2");
  header.AppendHeader("key2", "value_3");
  header.AppendHeader("key1", "value_4");
  header.AppendHeader("key2", "value_5");
  header.AppendHeader("key1", "value_6");
  ASSERT_NE(header.lines().begin(), header.lines().end());

  BalsaHeaders::const_header_lines_iterator chli = header.lines().begin();
  ++chli;
  ++chli;
  ++chli;
  header.erase(chli);

  chli = header.lines().begin();
  ++chli;
  header.erase(chli);

  header.RemoveAllOfHeader("key1");
  for (const auto& line : header.lines()) {
    EXPECT_NE(std::string("key1"), line.first);
  }
}

TEST(BalsaHeaders, RemoveAllOfHeaderDoesNothingWhenNoKeyOfThatNameExists) {
  BalsaHeaders header;
  header.AppendHeader("key", "value_1");
  header.AppendHeader("key", "value_2");
  ASSERT_NE(header.lines().begin(), header.lines().end());
  header.RemoveAllOfHeader("foo");
  int num_found = 0;
  for (const auto& line : header.lines()) {
    ++num_found;
    EXPECT_EQ(absl::string_view("key"), line.first);
  }
  EXPECT_EQ(2, num_found);
  EXPECT_NE(header.lines().begin(), header.lines().end());
}

TEST(BalsaHeaders, WriteHeaderEndingToBuffer) {
  BalsaHeaders header;
  SimpleBuffer simple_buffer;
  header.WriteHeaderEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq("\r\n"));
}

TEST(BalsaHeaders, WriteToBufferDoesntCrashWithUninitializedHeader) {
  BalsaHeaders header;
  SimpleBuffer simple_buffer;
  header.WriteHeaderAndEndingToBuffer(&simple_buffer);
}

TEST(BalsaHeaders, WriteToBufferWorksWithBalsaHeadersParsedByFramer) {
  std::string input =
      "GET / HTTP/1.0\r\n"
      "key_with_value: value\r\n"
      "key_with_continuation_value: \r\n"
      " with continuation\r\n"
      "key_with_two_continuation_value: \r\n"
      " continuation 1\r\n"
      " continuation 2\r\n"
      "a: foo    \r\n"
      "b-s:\n"
      " bar\t\n"
      "foo: \r\n"
      "bazzzzzzzleriffic!: snaps\n"
      "\n";
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key_with_value: value\r\n"
      "key_with_continuation_value: with continuation\r\n"
      "key_with_two_continuation_value: continuation 1\r\n"
      " continuation 2\r\n"
      "a: foo\r\n"
      "b-s: bar\r\n"
      "foo: \r\n"
      "bazzzzzzzleriffic!: snaps\r\n"
      "\r\n";

  BalsaHeaders headers = CreateHTTPHeaders(true, input);
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders,
     WriteToBufferWorksWithBalsaHeadersParsedByFramerTabContinuations) {
  std::string input =
      "GET / HTTP/1.0\r\n"
      "key_with_value: value\r\n"
      "key_with_continuation_value: \r\n"
      "\twith continuation\r\n"
      "key_with_two_continuation_value: \r\n"
      "\tcontinuation 1\r\n"
      "\tcontinuation 2\r\n"
      "a: foo    \r\n"
      "b-s:\n"
      "\tbar\t\n"
      "foo: \r\n"
      "bazzzzzzzleriffic!: snaps\n"
      "\n";
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key_with_value: value\r\n"
      "key_with_continuation_value: with continuation\r\n"
      "key_with_two_continuation_value: continuation 1\r\n"
      "\tcontinuation 2\r\n"
      "a: foo\r\n"
      "b-s: bar\r\n"
      "foo: \r\n"
      "bazzzzzzzleriffic!: snaps\r\n"
      "\r\n";

  BalsaHeaders headers = CreateHTTPHeaders(true, input);
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, WriteToBufferWorksWhenFirstlineSetThroughHeaders) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "\r\n";
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, WriteToBufferWorksWhenSetThroughHeaders) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  headers.AppendHeader("key\n 3", "value3");
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key1: value1\r\n"
      "key 2: value\n"
      " 2\r\n"
      "key\n"
      " 3: value3\r\n"
      "\r\n";
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, WriteToBufferWorkWhensOnlyLinesSetThroughHeaders) {
  BalsaHeaders headers;
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  headers.AppendHeader("key\n 3", "value3");
  std::string expected =
      "\r\n"
      "key1: value1\r\n"
      "key 2: value\n"
      " 2\r\n"
      "key\n"
      " 3: value3\r\n"
      "\r\n";
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, WriteToBufferWorksWhenSetThroughHeadersWithElementsErased) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  headers.AppendHeader("key\n 3", "value3");
  headers.RemoveAllOfHeader("key1");
  headers.RemoveAllOfHeader("key\n 3");
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key 2: value\n"
      " 2\r\n"
      "\r\n";
  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, WriteToBufferWithManuallyAppendedHeaderLine) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  std::string expected =
      "GET / HTTP/1.0\r\n"
      "key1: value1\r\n"
      "key 2: value\n"
      " 2\r\n"
      "key 3: value 3\r\n"
      "\r\n";

  SimpleBuffer simple_buffer;
  size_t expected_write_buffer_size = headers.GetSizeForWriteBuffer();
  headers.WriteToBuffer(&simple_buffer);
  headers.WriteHeaderLineToBuffer(&simple_buffer, "key 3", "value 3",
                                  BalsaHeaders::CaseOption::kNoModification);
  headers.WriteHeaderEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected));
  EXPECT_EQ(expected_write_buffer_size + 16,
            static_cast<size_t>(simple_buffer.ReadableBytes()));
}

TEST(BalsaHeaders, DumpToStringEmptyHeaders) {
  BalsaHeaders headers;
  std::string headers_str;
  headers.DumpToString(&headers_str);
  EXPECT_EQ("\n <empty header>\n", headers_str);
}

TEST(BalsaHeaders, DumpToStringParsedHeaders) {
  std::string input =
      "GET / HTTP/1.0\r\n"
      "Header1: value\r\n"
      "Header2: value\r\n"
      "\r\n";
  std::string output =
      "\n"
      " GET / HTTP/1.0\n"
      " Header1: value\n"
      " Header2: value\n";

  BalsaHeaders headers = CreateHTTPHeaders(true, input);
  std::string headers_str;
  headers.DumpToString(&headers_str);
  EXPECT_EQ(output, headers_str);
  EXPECT_TRUE(headers.FramerIsDoneWriting());
}

TEST(BalsaHeaders, DumpToStringPartialHeaders) {
  BalsaHeaders headers;
  BalsaFrame balsa_frame;
  balsa_frame.set_is_request(true);
  balsa_frame.set_balsa_headers(&headers);
  std::string input =
      "GET / HTTP/1.0\r\n"
      "Header1: value\r\n"
      "Header2: value\r\n";
  std::string output = absl::StrFormat("\n <incomplete header len: %d>\n ",
                                       static_cast<int>(input.size()));
  output += input;
  output += '\n';

  ASSERT_EQ(input.size(), balsa_frame.ProcessInput(input.data(), input.size()));
  ASSERT_FALSE(balsa_frame.MessageFullyRead());
  std::string headers_str;
  headers.DumpToString(&headers_str);
  EXPECT_EQ(output, headers_str);
  EXPECT_FALSE(headers.FramerIsDoneWriting());
}

TEST(BalsaHeaders, DumpToStringParsingNonHeadersData) {
  BalsaHeaders headers;
  BalsaFrame balsa_frame;
  balsa_frame.set_is_request(true);
  balsa_frame.set_balsa_headers(&headers);
  std::string input =
      "This is not a header. "
      "Just some random data to simulate mismatch.";
  std::string output = absl::StrFormat("\n <incomplete header len: %d>\n ",
                                       static_cast<int>(input.size()));
  output += input;
  output += '\n';

  ASSERT_EQ(input.size(), balsa_frame.ProcessInput(input.data(), input.size()));
  ASSERT_FALSE(balsa_frame.MessageFullyRead());
  std::string headers_str;
  headers.DumpToString(&headers_str);
  EXPECT_EQ(output, headers_str);
}

TEST(BalsaHeaders, Clear) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  headers.AppendHeader("key\n 3", "value3");
  headers.RemoveAllOfHeader("key1");
  headers.RemoveAllOfHeader("key\n 3");
  headers.Clear();
  EXPECT_TRUE(headers.first_line().empty());
  EXPECT_EQ(headers.lines().begin(), headers.lines().end());
  EXPECT_TRUE(headers.IsEmpty());
}

TEST(BalsaHeaders,
     TestSetFromStringPiecesWithInitialFirstlineInHeaderStreamAndNewToo) {
  BalsaHeaders headers = CreateHTTPHeaders(false,
                                           "HTTP/1.1 200 reason phrase\r\n"
                                           "content-length: 0\r\n"
                                           "\r\n");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));

  headers.SetResponseFirstline("HTTP/1.0", 404, "a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
  EXPECT_THAT(headers.response_code(), StrEq("404"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(404));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("a reason"));
  EXPECT_THAT(headers.first_line(), StrEq("HTTP/1.0 404 a reason"));
}

TEST(BalsaHeaders,
     TestSetFromStringPiecesWithInitialFirstlineInHeaderStreamButNotNew) {
  BalsaHeaders headers = CreateHTTPHeaders(false,
                                           "HTTP/1.1 200 reason phrase\r\n"
                                           "content-length: 0\r\n"
                                           "\r\n");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));

  headers.SetResponseFirstline("HTTP/1.000", 404000,
                               "supercalifragilisticexpealidocious");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.000"));
  EXPECT_THAT(headers.response_code(), StrEq("404000"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(404000));
  EXPECT_THAT(headers.response_reason_phrase(),
              StrEq("supercalifragilisticexpealidocious"));
  EXPECT_THAT(headers.first_line(),
              StrEq("HTTP/1.000 404000 supercalifragilisticexpealidocious"));
}

TEST(BalsaHeaders,
     TestSetFromStringPiecesWithFirstFirstlineInHeaderStreamButNotNew2) {
  SCOPED_TRACE(
      "This test tests the codepath where the new firstline is"
      " too large to fit within the space used by the original"
      " firstline, but large enuogh to space in the free space"
      " available in both firstline plus the space made available"
      " with deleted header lines (specifically, the first one");
  BalsaHeaders headers = CreateHTTPHeaders(
      false,
      "HTTP/1.1 200 reason phrase\r\n"
      "a: 0987123409871234078130948710938471093827401983740198327401982374\r\n"
      "content-length: 0\r\n"
      "\r\n");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));

  headers.erase(headers.lines().begin());
  headers.SetResponseFirstline("HTTP/1.000", 404000,
                               "supercalifragilisticexpealidocious");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.000"));
  EXPECT_THAT(headers.response_code(), StrEq("404000"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(404000));
  EXPECT_THAT(headers.response_reason_phrase(),
              StrEq("supercalifragilisticexpealidocious"));
  EXPECT_THAT(headers.first_line(),
              StrEq("HTTP/1.000 404000 supercalifragilisticexpealidocious"));
}

TEST(BalsaHeaders, TestSetFirstlineFromStringPiecesWithNoInitialFirstline) {
  BalsaHeaders headers;
  headers.SetResponseFirstline("HTTP/1.1", 200, "don't need a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(200));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("don't need a reason"));
  EXPECT_THAT(headers.first_line(), StrEq("HTTP/1.1 200 don't need a reason"));
}

TEST(BalsaHeaders, TestSettingFirstlineElementsWithOtherElementsMissing) {
  {
    BalsaHeaders headers;
    headers.SetRequestMethod("GET");
    headers.SetRequestUri("/");
    EXPECT_THAT(headers.first_line(), StrEq("GET / "));
  }
  {
    BalsaHeaders headers;
    headers.SetRequestMethod("GET");
    headers.SetRequestVersion("HTTP/1.1");
    EXPECT_THAT(headers.first_line(), StrEq("GET  HTTP/1.1"));
  }
  {
    BalsaHeaders headers;
    headers.SetRequestUri("/");
    headers.SetRequestVersion("HTTP/1.1");
    EXPECT_THAT(headers.first_line(), StrEq(" / HTTP/1.1"));
  }
}

TEST(BalsaHeaders, TestSettingMissingFirstlineElementsAfterBalsaHeadersParsed) {
  {
    BalsaHeaders headers = CreateHTTPHeaders(true, "GET /foo\r\n");
    ASSERT_THAT(headers.first_line(), StrEq("GET /foo"));

    headers.SetRequestVersion("HTTP/1.1");
    EXPECT_THAT(headers.first_line(), StrEq("GET /foo HTTP/1.1"));
  }
  {
    BalsaHeaders headers = CreateHTTPHeaders(true, "GET\r\n");
    ASSERT_THAT(headers.first_line(), StrEq("GET"));

    headers.SetRequestUri("/foo");
    EXPECT_THAT(headers.first_line(), StrEq("GET /foo "));
  }
}

// Here we exersize the codepaths involved in setting a new firstine when the
// previously set firstline is stored in the 'additional_data_stream_'
// variable, and the new firstline is larger than the previously set firstline.
TEST(BalsaHeaders,
     SetFirstlineFromStringPiecesFirstInAdditionalDataAndNewLarger) {
  BalsaHeaders headers;
  // This one will end up being put into the additional data stream
  headers.SetResponseFirstline("HTTP/1.1", 200, "don't need a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(200));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("don't need a reason"));
  EXPECT_THAT(headers.first_line(), StrEq("HTTP/1.1 200 don't need a reason"));

  // Now, we set it again, this time we're extending what exists
  // here.
  headers.SetResponseFirstline("HTTP/1.10", 2000, "REALLY don't need a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.10"));
  EXPECT_THAT(headers.response_code(), StrEq("2000"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(2000));
  EXPECT_THAT(headers.response_reason_phrase(),
              StrEq("REALLY don't need a reason"));
  EXPECT_THAT(headers.first_line(),
              StrEq("HTTP/1.10 2000 REALLY don't need a reason"));
}

// Here we exersize the codepaths involved in setting a new firstine when the
// previously set firstline is stored in the 'additional_data_stream_'
// variable, and the new firstline is smaller than the previously set firstline.
TEST(BalsaHeaders,
     TestSetFirstlineFromStringPiecesWithPreviousInAdditionalDataNewSmaller) {
  BalsaHeaders headers;
  // This one will end up being put into the additional data stream
  //
  headers.SetResponseFirstline("HTTP/1.10", 2000, "REALLY don't need a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.10"));
  EXPECT_THAT(headers.response_code(), StrEq("2000"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(2000));
  EXPECT_THAT(headers.response_reason_phrase(),
              StrEq("REALLY don't need a reason"));
  EXPECT_THAT(headers.first_line(),
              StrEq("HTTP/1.10 2000 REALLY don't need a reason"));

  // Now, we set it again, this time we're extending what exists
  // here.
  headers.SetResponseFirstline("HTTP/1.0", 200, "a reason");
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
  EXPECT_THAT(headers.response_code(), StrEq("200"));
  EXPECT_THAT(headers.parsed_response_code(), Eq(200));
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("a reason"));
  EXPECT_THAT(headers.first_line(), StrEq("HTTP/1.0 200 a reason"));
}

TEST(BalsaHeaders, CopyFrom) {
  BalsaHeaders headers1, headers2;
  absl::string_view method("GET");
  absl::string_view uri("/foo");
  absl::string_view version("HTTP/1.0");
  headers1.SetRequestFirstlineFromStringPieces(method, uri, version);
  headers1.AppendHeader("key1", "value1");
  headers1.AppendHeader("key 2", "value\n 2");
  headers1.AppendHeader("key\n 3", "value3");

  // "GET /foo HTTP/1.0"     // 17
  // "key1: value1\r\n"      // 14
  // "key 2: value\n 2\r\n"  // 17
  // "key\n 3: value3\r\n"   // 16

  headers2.CopyFrom(headers1);

  EXPECT_THAT(headers1.first_line(), StrEq("GET /foo HTTP/1.0"));
  BalsaHeaders::const_header_lines_iterator chli = headers1.lines().begin();
  EXPECT_THAT(chli->first, StrEq("key1"));
  EXPECT_THAT(chli->second, StrEq("value1"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key 2"));
  EXPECT_THAT(chli->second, StrEq("value\n 2"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key\n 3"));
  EXPECT_THAT(chli->second, StrEq("value3"));
  ++chli;
  EXPECT_EQ(headers1.lines().end(), chli);

  EXPECT_THAT(headers1.request_method(),
              StrEq((std::string(headers2.request_method()))));
  EXPECT_THAT(headers1.request_uri(),
              StrEq((std::string(headers2.request_uri()))));
  EXPECT_THAT(headers1.request_version(),
              StrEq((std::string(headers2.request_version()))));

  EXPECT_THAT(headers2.first_line(), StrEq("GET /foo HTTP/1.0"));
  chli = headers2.lines().begin();
  EXPECT_THAT(chli->first, StrEq("key1"));
  EXPECT_THAT(chli->second, StrEq("value1"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key 2"));
  EXPECT_THAT(chli->second, StrEq("value\n 2"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key\n 3"));
  EXPECT_THAT(chli->second, StrEq("value3"));
  ++chli;
  EXPECT_EQ(headers2.lines().end(), chli);

  version = absl::string_view("HTTP/1.1");
  int code = 200;
  absl::string_view reason_phrase("reason phrase asdf");

  headers1.RemoveAllOfHeader("key1");
  headers1.AppendHeader("key4", "value4");

  headers1.SetResponseFirstline(version, code, reason_phrase);

  headers2.CopyFrom(headers1);

  // "GET /foo HTTP/1.0"     // 17
  // "XXXXXXXXXXXXXX"        // 14
  // "key 2: value\n 2\r\n"  // 17
  // "key\n 3: value3\r\n"   // 16
  // "key4: value4\r\n"      // 14
  //
  //       ->
  //
  // "HTTP/1.1 200 reason phrase asdf"  // 31 = (17 + 14)
  // "key 2: value\n 2\r\n"             // 17
  // "key\n 3: value3\r\n"              // 16
  // "key4: value4\r\n"                 // 14

  EXPECT_THAT(headers1.request_method(),
              StrEq((std::string(headers2.request_method()))));
  EXPECT_THAT(headers1.request_uri(),
              StrEq((std::string(headers2.request_uri()))));
  EXPECT_THAT(headers1.request_version(),
              StrEq((std::string(headers2.request_version()))));

  EXPECT_THAT(headers2.first_line(), StrEq("HTTP/1.1 200 reason phrase asdf"));
  chli = headers2.lines().begin();
  EXPECT_THAT(chli->first, StrEq("key 2"));
  EXPECT_THAT(chli->second, StrEq("value\n 2"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key\n 3"));
  EXPECT_THAT(chli->second, StrEq("value3"));
  ++chli;
  EXPECT_THAT(chli->first, StrEq("key4"));
  EXPECT_THAT(chli->second, StrEq("value4"));
  ++chli;
  EXPECT_EQ(headers2.lines().end(), chli);
}

// Test BalsaHeaders move constructor and move assignment operator.
TEST(BalsaHeaders, Move) {
  BalsaHeaders headers1, headers3;
  absl::string_view method("GET");
  absl::string_view uri("/foo");
  absl::string_view version("HTTP/1.0");
  headers1.SetRequestFirstlineFromStringPieces(method, uri, version);
  headers1.AppendHeader("key1", "value1");
  headers1.AppendHeader("key 2", "value\n 2");
  headers1.AppendHeader("key\n 3", "value3");

  // "GET /foo HTTP/1.0"     // 17
  // "key1: value1\r\n"      // 14
  // "key 2: value\n 2\r\n"  // 17
  // "key\n 3: value3\r\n"   // 16

  BalsaHeaders headers2 = std::move(headers1);

  EXPECT_EQ("GET /foo HTTP/1.0", headers2.first_line());
  BalsaHeaders::const_header_lines_iterator chli = headers2.lines().begin();
  EXPECT_EQ("key1", chli->first);
  EXPECT_EQ("value1", chli->second);
  ++chli;
  EXPECT_EQ("key 2", chli->first);
  EXPECT_EQ("value\n 2", chli->second);
  ++chli;
  EXPECT_EQ("key\n 3", chli->first);
  EXPECT_EQ("value3", chli->second);
  ++chli;
  EXPECT_EQ(headers2.lines().end(), chli);

  EXPECT_EQ("GET", headers2.request_method());
  EXPECT_EQ("/foo", headers2.request_uri());
  EXPECT_EQ("HTTP/1.0", headers2.request_version());

  headers3 = std::move(headers2);
  version = absl::string_view("HTTP/1.1");
  int code = 200;
  absl::string_view reason_phrase("reason phrase asdf");

  headers3.RemoveAllOfHeader("key1");
  headers3.AppendHeader("key4", "value4");

  headers3.SetResponseFirstline(version, code, reason_phrase);

  BalsaHeaders headers4 = std::move(headers3);

  // "GET /foo HTTP/1.0"     // 17
  // "XXXXXXXXXXXXXX"        // 14
  // "key 2: value\n 2\r\n"  // 17
  // "key\n 3: value3\r\n"   // 16
  // "key4: value4\r\n"      // 14
  //
  //       ->
  //
  // "HTTP/1.1 200 reason phrase asdf"  // 31 = (17 + 14)
  // "key 2: value\n 2\r\n"             // 17
  // "key\n 3: value3\r\n"              // 16
  // "key4: value4\r\n"                 // 14

  EXPECT_EQ("200", headers4.response_code());
  EXPECT_EQ("reason phrase asdf", headers4.response_reason_phrase());
  EXPECT_EQ("HTTP/1.1", headers4.response_version());

  EXPECT_EQ("HTTP/1.1 200 reason phrase asdf", headers4.first_line());
  chli = headers4.lines().begin();
  EXPECT_EQ("key 2", chli->first);
  EXPECT_EQ("value\n 2", chli->second);
  ++chli;
  EXPECT_EQ("key\n 3", chli->first);
  EXPECT_EQ("value3", chli->second);
  ++chli;
  EXPECT_EQ("key4", chli->first);
  EXPECT_EQ("value4", chli->second);
  ++chli;
  EXPECT_EQ(headers4.lines().end(), chli);
}

TEST(BalsaHeaders, IteratorWorksWithOStreamAsExpected) {
  {
    std::stringstream actual;
    BalsaHeaders::const_header_lines_iterator chli;
    actual << chli;
    // Note that the output depends on the flavor of standard library in use.
    EXPECT_THAT(actual.str(), AnyOf(StrEq("[0, 0]"),      // libstdc++
                                    StrEq("[(nil), 0]"),  // libc++
                                    StrEq("[0x0, 0]")));  // libc++ on Mac
  }
  {
    BalsaHeaders headers;
    std::stringstream actual;
    BalsaHeaders::const_header_lines_iterator chli = headers.lines().begin();
    actual << chli;
    std::stringstream expected;
    expected << "[" << &headers << ", 0]";
    EXPECT_THAT(expected.str(), StrEq(actual.str()));
  }
}

TEST(BalsaHeaders, TestSetResponseReasonPhraseWithNoInitialFirstline) {
  BalsaHeaders balsa_headers;
  balsa_headers.SetResponseReasonPhrase("don't need a reason");
  EXPECT_THAT(balsa_headers.first_line(), StrEq("  don't need a reason"));
  EXPECT_TRUE(balsa_headers.response_version().empty());
  EXPECT_TRUE(balsa_headers.response_code().empty());
  EXPECT_THAT(balsa_headers.response_reason_phrase(),
              StrEq("don't need a reason"));
}

// Testing each of 9 combinations separately was taking up way too much of this
// file (not to mention the inordinate amount of stupid code duplication), thus
// this test tests all 9 combinations of smaller, equal, and larger in one
// place.
TEST(BalsaHeaders, TestSetResponseReasonPhrase) {
  const char* response_reason_phrases[] = {
      "qwerty asdfgh",
      "qwerty",
      "qwerty asdfghjkl",
  };
  size_t arraysize_squared = (ABSL_ARRAYSIZE(response_reason_phrases) *
                              ABSL_ARRAYSIZE(response_reason_phrases));
  // We go through the 9 different permutations of (response_reason_phrases
  // choose 2) in the loop below. For each permutation, we mutate the firstline
  // twice-- once from the original, and once from the previous.
  for (size_t iteration = 0; iteration < arraysize_squared; ++iteration) {
    SCOPED_TRACE("Original firstline: \"HTTP/1.0 200 reason phrase\"");
    BalsaHeaders headers = CreateHTTPHeaders(true,
                                             "HTTP/1.0 200 reason phrase\r\n"
                                             "content-length: 0\r\n"
                                             "\r\n");
    ASSERT_THAT(headers.first_line(), StrEq("HTTP/1.0 200 reason phrase"));

    {
      int first = iteration / ABSL_ARRAYSIZE(response_reason_phrases);
      const char* response_reason_phrase_first = response_reason_phrases[first];
      std::string expected_new_firstline =
          absl::StrFormat("HTTP/1.0 200 %s", response_reason_phrase_first);
      SCOPED_TRACE(absl::StrFormat("Then set response_reason_phrase(\"%s\")",
                                   response_reason_phrase_first));

      headers.SetResponseReasonPhrase(response_reason_phrase_first);
      EXPECT_THAT(headers.first_line(),
                  StrEq(absl::StrFormat("HTTP/1.0 200 %s",
                                        response_reason_phrase_first)));
      EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
      EXPECT_THAT(headers.response_code(), StrEq("200"));
      EXPECT_THAT(headers.response_reason_phrase(),
                  StrEq(response_reason_phrase_first));
    }

    // Note that each iteration of the outer loop causes the headers to be left
    // in a different state. Nothing wrong with that, but we should use each of
    // these states, and try each of our scenarios again. This inner loop does
    // that.
    {
      int second = iteration % ABSL_ARRAYSIZE(response_reason_phrases);
      const char* response_reason_phrase_second =
          response_reason_phrases[second];
      std::string expected_new_firstline =
          absl::StrFormat("HTTP/1.0 200 %s", response_reason_phrase_second);
      SCOPED_TRACE(absl::StrFormat("Then set response_reason_phrase(\"%s\")",
                                   response_reason_phrase_second));

      headers.SetResponseReasonPhrase(response_reason_phrase_second);
      EXPECT_THAT(headers.first_line(),
                  StrEq(absl::StrFormat("HTTP/1.0 200 %s",
                                        response_reason_phrase_second)));
      EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
      EXPECT_THAT(headers.response_code(), StrEq("200"));
      EXPECT_THAT(headers.response_reason_phrase(),
                  StrEq(response_reason_phrase_second));
    }
  }
}

TEST(BalsaHeaders, TestSetResponseVersionWithNoInitialFirstline) {
  BalsaHeaders balsa_headers;
  balsa_headers.SetResponseVersion("HTTP/1.1");
  EXPECT_THAT(balsa_headers.first_line(), StrEq("HTTP/1.1  "));
  EXPECT_THAT(balsa_headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_TRUE(balsa_headers.response_code().empty());
  EXPECT_TRUE(balsa_headers.response_reason_phrase().empty());
}

// Testing each of 9 combinations separately was taking up way too much of this
// file (not to mention the inordinate amount of stupid code duplication), thus
// this test tests all 9 combinations of smaller, equal, and larger in one
// place.
TEST(BalsaHeaders, TestSetResponseVersion) {
  const char* response_versions[] = {
      "ABCD/123",
      "ABCD",
      "ABCD/123456",
  };
  size_t arraysize_squared =
      (ABSL_ARRAYSIZE(response_versions) * ABSL_ARRAYSIZE(response_versions));
  // We go through the 9 different permutations of (response_versions choose 2)
  // in the loop below. For each permutation, we mutate the firstline twice--
  // once from the original, and once from the previous.
  for (size_t iteration = 0; iteration < arraysize_squared; ++iteration) {
    SCOPED_TRACE("Original firstline: \"HTTP/1.0 200 reason phrase\"");
    BalsaHeaders headers = CreateHTTPHeaders(false,
                                             "HTTP/1.0 200 reason phrase\r\n"
                                             "content-length: 0\r\n"
                                             "\r\n");
    ASSERT_THAT(headers.first_line(), StrEq("HTTP/1.0 200 reason phrase"));

    // This structure guarantees that we'll visit all of the possible
    // variations of setting.

    {
      int first = iteration / ABSL_ARRAYSIZE(response_versions);
      const char* response_version_first = response_versions[first];
      std::string expected_new_firstline =
          absl::StrFormat("%s 200 reason phrase", response_version_first);
      SCOPED_TRACE(absl::StrFormat("Then set response_version(\"%s\")",
                                   response_version_first));

      headers.SetResponseVersion(response_version_first);
      EXPECT_THAT(headers.first_line(), StrEq(expected_new_firstline));
      EXPECT_THAT(headers.response_version(), StrEq(response_version_first));
      EXPECT_THAT(headers.response_code(), StrEq("200"));
      EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));
    }
    {
      int second = iteration % ABSL_ARRAYSIZE(response_versions);
      const char* response_version_second = response_versions[second];
      std::string expected_new_firstline =
          absl::StrFormat("%s 200 reason phrase", response_version_second);
      SCOPED_TRACE(absl::StrFormat("Then set response_version(\"%s\")",
                                   response_version_second));

      headers.SetResponseVersion(response_version_second);
      EXPECT_THAT(headers.first_line(), StrEq(expected_new_firstline));
      EXPECT_THAT(headers.response_version(), StrEq(response_version_second));
      EXPECT_THAT(headers.response_code(), StrEq("200"));
      EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));
    }
  }
}

TEST(BalsaHeaders, TestSetResponseReasonAndVersionWithNoInitialFirstline) {
  BalsaHeaders headers;
  headers.SetResponseVersion("HTTP/1.1");
  headers.SetResponseReasonPhrase("don't need a reason");
  EXPECT_THAT(headers.first_line(), StrEq("HTTP/1.1  don't need a reason"));
  EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.1"));
  EXPECT_TRUE(headers.response_code().empty());
  EXPECT_THAT(headers.response_reason_phrase(), StrEq("don't need a reason"));
}

TEST(BalsaHeaders, TestSetResponseCodeWithNoInitialFirstline) {
  BalsaHeaders balsa_headers;
  balsa_headers.SetParsedResponseCodeAndUpdateFirstline(2002);
  EXPECT_THAT(balsa_headers.first_line(), StrEq(" 2002 "));
  EXPECT_TRUE(balsa_headers.response_version().empty());
  EXPECT_THAT(balsa_headers.response_code(), StrEq("2002"));
  EXPECT_TRUE(balsa_headers.response_reason_phrase().empty());
  EXPECT_THAT(balsa_headers.parsed_response_code(), Eq(2002));
}

TEST(BalsaHeaders, TestSetParsedResponseCode) {
  BalsaHeaders balsa_headers;
  balsa_headers.set_parsed_response_code(std::numeric_limits<int>::max());
  EXPECT_THAT(balsa_headers.parsed_response_code(),
              Eq(std::numeric_limits<int>::max()));
}

TEST(BalsaHeaders, TestSetResponseCode) {
  const char* response_codes[] = {
      "200"
      "23",
      "200200",
  };
  size_t arraysize_squared =
      (ABSL_ARRAYSIZE(response_codes) * ABSL_ARRAYSIZE(response_codes));
  // We go through the 9 different permutations of (response_codes choose 2)
  // in the loop below. For each permutation, we mutate the firstline twice--
  // once from the original, and once from the previous.
  for (size_t iteration = 0; iteration < arraysize_squared; ++iteration) {
    SCOPED_TRACE("Original firstline: \"HTTP/1.0 200 reason phrase\"");
    BalsaHeaders headers = CreateHTTPHeaders(false,
                                             "HTTP/1.0 200 reason phrase\r\n"
                                             "content-length: 0\r\n"
                                             "\r\n");
    ASSERT_THAT(headers.first_line(), StrEq("HTTP/1.0 200 reason phrase"));

    // This structure guarantees that we'll visit all of the possible
    // variations of setting.

    {
      int first = iteration / ABSL_ARRAYSIZE(response_codes);
      const char* response_code_first = response_codes[first];
      std::string expected_new_firstline =
          absl::StrFormat("HTTP/1.0 %s reason phrase", response_code_first);
      SCOPED_TRACE(absl::StrFormat("Then set response_code(\"%s\")",
                                   response_code_first));

      headers.SetResponseCode(response_code_first);

      EXPECT_THAT(headers.first_line(), StrEq(expected_new_firstline));
      EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
      EXPECT_THAT(headers.response_code(), StrEq(response_code_first));
      EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));
    }
    {
      int second = iteration % ABSL_ARRAYSIZE(response_codes);
      const char* response_code_second = response_codes[second];
      std::string expected_new_secondline =
          absl::StrFormat("HTTP/1.0 %s reason phrase", response_code_second);
      SCOPED_TRACE(absl::StrFormat("Then set response_code(\"%s\")",
                                   response_code_second));

      headers.SetResponseCode(response_code_second);

      EXPECT_THAT(headers.first_line(), StrEq(expected_new_secondline));
      EXPECT_THAT(headers.response_version(), StrEq("HTTP/1.0"));
      EXPECT_THAT(headers.response_code(), StrEq(response_code_second));
      EXPECT_THAT(headers.response_reason_phrase(), StrEq("reason phrase"));
    }
  }
}

TEST(BalsaHeaders, TestAppendToHeader) {
  // Test the basic case of appending to a header.
  BalsaHeaders headers;
  headers.AppendHeader("foo", "foo_value");
  headers.AppendHeader("bar", "bar_value");
  headers.AppendToHeader("foo", "foo_value2");

  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value,foo_value2"));
  EXPECT_THAT(headers.GetHeader("bar"), StrEq("bar_value"));
}

TEST(BalsaHeaders, TestInitialAppend) {
  // Test that AppendToHeader works properly when the header did not already
  // exist.
  BalsaHeaders headers;
  headers.AppendToHeader("foo", "foo_value");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value"));
  headers.AppendToHeader("foo", "foo_value2");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value,foo_value2"));
}

TEST(BalsaHeaders, TestAppendAndRemove) {
  // Test that AppendToHeader works properly with removing.
  BalsaHeaders headers;
  headers.AppendToHeader("foo", "foo_value");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value"));
  headers.AppendToHeader("foo", "foo_value2");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value,foo_value2"));
  headers.RemoveAllOfHeader("foo");
  headers.AppendToHeader("foo", "foo_value3");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value3"));
  headers.AppendToHeader("foo", "foo_value4");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value3,foo_value4"));
}

TEST(BalsaHeaders, TestAppendToHeaderWithCommaAndSpace) {
  // Test the basic case of appending to a header with comma and space.
  BalsaHeaders headers;
  headers.AppendHeader("foo", "foo_value");
  headers.AppendHeader("bar", "bar_value");
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value2");

  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value, foo_value2"));
  EXPECT_THAT(headers.GetHeader("bar"), StrEq("bar_value"));
}

TEST(BalsaHeaders, TestInitialAppendWithCommaAndSpace) {
  // Test that AppendToHeadeWithCommaAndSpace works properly when the
  // header did not already exist.
  BalsaHeaders headers;
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value"));
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value2");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value, foo_value2"));
}

TEST(BalsaHeaders, TestAppendWithCommaAndSpaceAndRemove) {
  // Test that AppendToHeadeWithCommaAndSpace works properly with removing.
  BalsaHeaders headers;
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value"));
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value2");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value, foo_value2"));
  headers.RemoveAllOfHeader("foo");
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value3");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value3"));
  headers.AppendToHeaderWithCommaAndSpace("foo", "foo_value4");
  EXPECT_THAT(headers.GetHeader("foo"), StrEq("foo_value3, foo_value4"));
}

TEST(BalsaHeaders, SetContentLength) {
  // Test that SetContentLength correctly sets the content-length header and
  // sets the content length status.
  BalsaHeaders headers;
  headers.SetContentLength(10);
  EXPECT_THAT(headers.GetHeader("Content-length"), StrEq("10"));
  EXPECT_EQ(BalsaHeadersEnums::VALID_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_TRUE(headers.content_length_valid());

  // Test overwriting the content-length.
  headers.SetContentLength(0);
  EXPECT_THAT(headers.GetHeader("Content-length"), StrEq("0"));
  EXPECT_EQ(BalsaHeadersEnums::VALID_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_TRUE(headers.content_length_valid());

  // Make sure there is only one header line after the overwrite.
  BalsaHeaders::const_header_lines_iterator iter =
      headers.GetHeaderPosition("Content-length");
  EXPECT_EQ(headers.lines().begin(), iter);
  EXPECT_EQ(headers.lines().end(), ++iter);

  // Test setting the same content-length again, this should be no-op.
  headers.SetContentLength(0);
  EXPECT_THAT(headers.GetHeader("Content-length"), StrEq("0"));
  EXPECT_EQ(BalsaHeadersEnums::VALID_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_TRUE(headers.content_length_valid());

  // Make sure the number of header lines didn't change.
  iter = headers.GetHeaderPosition("Content-length");
  EXPECT_EQ(headers.lines().begin(), iter);
  EXPECT_EQ(headers.lines().end(), ++iter);
}

TEST(BalsaHeaders, ToggleChunkedEncoding) {
  // Test that SetTransferEncodingToChunkedAndClearContentLength correctly adds
  // chunk-encoding header and sets the transfer_encoding_is_chunked_
  // flag.
  BalsaHeaders headers;
  headers.SetTransferEncodingToChunkedAndClearContentLength();
  EXPECT_EQ("chunked", headers.GetAllOfHeaderAsString("Transfer-Encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("Transfer-Encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("transfer-encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("transfer"));
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());

  // Set it to the same value, nothing should change.
  headers.SetTransferEncodingToChunkedAndClearContentLength();
  EXPECT_EQ("chunked", headers.GetAllOfHeaderAsString("Transfer-Encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("Transfer-Encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("transfer-encoding"));
  EXPECT_TRUE(headers.HasHeadersWithPrefix("transfer"));
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());
  BalsaHeaders::const_header_lines_iterator iter =
      headers.GetHeaderPosition("Transfer-Encoding");
  EXPECT_EQ(headers.lines().begin(), iter);
  EXPECT_EQ(headers.lines().end(), ++iter);

  // Removes the chunked encoding, and there should be no transfer-encoding
  // headers left.
  headers.SetNoTransferEncoding();
  EXPECT_FALSE(headers.HasHeader("Transfer-Encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("Transfer-Encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("transfer-encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("transfer"));
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());
  EXPECT_EQ(headers.lines().end(), headers.lines().begin());

  // Clear chunked again, this should be a no-op and the header should not
  // change.
  headers.SetNoTransferEncoding();
  EXPECT_FALSE(headers.HasHeader("Transfer-Encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("Transfer-Encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("transfer-encoding"));
  EXPECT_FALSE(headers.HasHeadersWithPrefix("transfer"));
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());
  EXPECT_EQ(headers.lines().end(), headers.lines().begin());
}

TEST(BalsaHeaders, SetNoTransferEncodingByRemoveHeader) {
  // Tests that calling Remove() methods to clear the Transfer-Encoding
  // header correctly resets transfer_encoding_is_chunked_ internal state.
  BalsaHeaders headers;
  headers.SetTransferEncodingToChunkedAndClearContentLength();
  headers.RemoveAllOfHeader("Transfer-Encoding");
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());

  headers.SetTransferEncodingToChunkedAndClearContentLength();
  std::vector<absl::string_view> headers_to_remove;
  headers_to_remove.emplace_back("Transfer-Encoding");
  headers.RemoveAllOfHeaderInList(headers_to_remove);
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());

  headers.SetTransferEncodingToChunkedAndClearContentLength();
  headers.RemoveAllHeadersWithPrefix("Transfer");
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());
}

TEST(BalsaHeaders, ClearContentLength) {
  // Test that ClearContentLength() removes the content-length header and
  // resets content_length_status().
  BalsaHeaders headers;
  headers.SetContentLength(10);
  headers.ClearContentLength();
  EXPECT_FALSE(headers.HasHeader("Content-length"));
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_FALSE(headers.content_length_valid());

  // Clear it again; nothing should change.
  headers.ClearContentLength();
  EXPECT_FALSE(headers.HasHeader("Content-length"));
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_FALSE(headers.content_length_valid());

  // Set chunked encoding and test that ClearContentLength() has no effect.
  headers.SetTransferEncodingToChunkedAndClearContentLength();
  headers.ClearContentLength();
  EXPECT_EQ("chunked", headers.GetAllOfHeaderAsString("Transfer-Encoding"));
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());
  BalsaHeaders::const_header_lines_iterator iter =
      headers.GetHeaderPosition("Transfer-Encoding");
  EXPECT_EQ(headers.lines().begin(), iter);
  EXPECT_EQ(headers.lines().end(), ++iter);

  // Remove chunked encoding, and verify that the state is the same as after
  // ClearContentLength().
  headers.SetNoTransferEncoding();
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_FALSE(headers.content_length_valid());
}

TEST(BalsaHeaders, ClearContentLengthByRemoveHeader) {
  // Test that calling Remove() methods to clear the content-length header
  // correctly resets internal content length fields.
  BalsaHeaders headers;
  headers.SetContentLength(10);
  headers.RemoveAllOfHeader("Content-Length");
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_EQ(0u, headers.content_length());
  EXPECT_FALSE(headers.content_length_valid());

  headers.SetContentLength(11);
  std::vector<absl::string_view> headers_to_remove;
  headers_to_remove.emplace_back("Content-Length");
  headers.RemoveAllOfHeaderInList(headers_to_remove);
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_EQ(0u, headers.content_length());
  EXPECT_FALSE(headers.content_length_valid());

  headers.SetContentLength(12);
  headers.RemoveAllHeadersWithPrefix("Content");
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_EQ(0u, headers.content_length());
  EXPECT_FALSE(headers.content_length_valid());
}

// Chunk-encoding an identity-coded BalsaHeaders removes the identity-coding.
TEST(BalsaHeaders, IdentityCodingToChunked) {
  std::string message =
      "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: identity\r\n\r\n";
  BalsaHeaders headers;
  BalsaFrame balsa_frame;
  balsa_frame.set_is_request(false);
  balsa_frame.set_balsa_headers(&headers);
  EXPECT_EQ(message.size(),
            balsa_frame.ProcessInput(message.data(), message.size()));

  EXPECT_TRUE(headers.is_framed_by_connection_close());
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());
  EXPECT_THAT(headers.GetAllOfHeader("Transfer-Encoding"),
              ElementsAre("identity"));

  headers.SetTransferEncodingToChunkedAndClearContentLength();

  EXPECT_FALSE(headers.is_framed_by_connection_close());
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());
  EXPECT_THAT(headers.GetAllOfHeader("Transfer-Encoding"),
              ElementsAre("chunked"));
}

TEST(BalsaHeaders, SwitchContentLengthToChunk) {
  // Test that a header originally with content length header is correctly
  // switched to using chunk encoding.
  BalsaHeaders headers;
  headers.SetContentLength(10);
  EXPECT_THAT(headers.GetHeader("Content-length"), StrEq("10"));
  EXPECT_EQ(BalsaHeadersEnums::VALID_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_TRUE(headers.content_length_valid());

  headers.SetTransferEncodingToChunkedAndClearContentLength();
  EXPECT_EQ("chunked", headers.GetAllOfHeaderAsString("Transfer-Encoding"));
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());
  EXPECT_FALSE(headers.HasHeader("Content-length"));
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_FALSE(headers.content_length_valid());
}

TEST(BalsaHeaders, SwitchChunkedToContentLength) {
  // Test that a header originally with chunk encoding is correctly
  // switched to using content length.
  BalsaHeaders headers;
  headers.SetTransferEncodingToChunkedAndClearContentLength();
  EXPECT_EQ("chunked", headers.GetAllOfHeaderAsString("Transfer-Encoding"));
  EXPECT_TRUE(headers.transfer_encoding_is_chunked());
  EXPECT_FALSE(headers.HasHeader("Content-length"));
  EXPECT_EQ(BalsaHeadersEnums::NO_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_FALSE(headers.content_length_valid());

  headers.SetContentLength(10);
  EXPECT_THAT(headers.GetHeader("Content-length"), StrEq("10"));
  EXPECT_EQ(BalsaHeadersEnums::VALID_CONTENT_LENGTH,
            headers.content_length_status());
  EXPECT_TRUE(headers.content_length_valid());
  EXPECT_FALSE(headers.HasHeader("Transfer-Encoding"));
  EXPECT_FALSE(headers.transfer_encoding_is_chunked());
}

TEST(BalsaHeaders, OneHundredResponseMessagesNoFramedByClose) {
  BalsaHeaders headers;
  headers.SetResponseFirstline("HTTP/1.1", 100, "Continue");
  EXPECT_FALSE(headers.is_framed_by_connection_close());
}

TEST(BalsaHeaders, TwoOhFourResponseMessagesNoFramedByClose) {
  BalsaHeaders headers;
  headers.SetResponseFirstline("HTTP/1.1", 204, "Continue");
  EXPECT_FALSE(headers.is_framed_by_connection_close());
}

TEST(BalsaHeaders, ThreeOhFourResponseMessagesNoFramedByClose) {
  BalsaHeaders headers;
  headers.SetResponseFirstline("HTTP/1.1", 304, "Continue");
  EXPECT_FALSE(headers.is_framed_by_connection_close());
}

TEST(BalsaHeaders, InvalidCharInHeaderValue) {
  std::string message =
      "GET http://www.256.com/foo HTTP/1.1\r\n"
      "Host: \x01\x01www.265.com\r\n"
      "\r\n";
  BalsaHeaders headers = CreateHTTPHeaders(true, message);
  EXPECT_EQ("www.265.com", headers.GetHeader("Host"));
  SimpleBuffer buffer;
  headers.WriteHeaderAndEndingToBuffer(&buffer);
  message.replace(message.find_first_of(0x1), 2, "");
  EXPECT_EQ(message, buffer.GetReadableRegion());
}

TEST(BalsaHeaders, CarriageReturnAtStartOfLine) {
  std::string message =
      "GET /foo HTTP/1.1\r\n"
      "Host: www.265.com\r\n"
      "Foo: bar\r\n"
      "\rX-User-Ip: 1.2.3.4\r\n"
      "\r\n";
  BalsaHeaders headers;
  BalsaFrame balsa_frame;
  balsa_frame.set_is_request(true);
  balsa_frame.set_balsa_headers(&headers);
  EXPECT_EQ(message.size(),
            balsa_frame.ProcessInput(message.data(), message.size()));
  EXPECT_EQ(BalsaFrameEnums::INVALID_HEADER_FORMAT, balsa_frame.ErrorCode());
  EXPECT_TRUE(balsa_frame.Error());
}

TEST(BalsaHeaders, CheckEmpty) {
  BalsaHeaders headers;
  EXPECT_TRUE(headers.IsEmpty());
}

TEST(BalsaHeaders, CheckNonEmpty) {
  BalsaHeaders headers;
  BalsaHeadersTestPeer::WriteFromFramer(&headers, "a b c", 5);
  EXPECT_FALSE(headers.IsEmpty());
}

TEST(BalsaHeaders, ForEachHeader) {
  BalsaHeaders headers;
  headers.AppendHeader(":host", "SomeHost");
  headers.AppendHeader("key", "val1,val2val2,val2,val3");
  headers.AppendHeader("key", "val4val5val6");
  headers.AppendHeader("key", "val11 val12");
  headers.AppendHeader("key", "v val13");
  headers.AppendHeader("key", "val7");
  headers.AppendHeader("key", "");
  headers.AppendHeader("key", "val8 , val9 ,, val10");
  headers.AppendHeader("key", " val14 ");
  headers.AppendHeader("key2", "val15");
  headers.AppendHeader("key", "Val16");
  headers.AppendHeader("key", "foo, Val17, bar");
  headers.AppendHeader("date", "2 Jan 1970");
  headers.AppendHeader("AcceptEncoding", "MyFavoriteEncoding");

  {
    std::string result;
    EXPECT_TRUE(headers.ForEachHeader(
        [&result](const absl::string_view key, absl::string_view value) {
          result.append("<")
              .append(key.data(), key.size())
              .append("> = <")
              .append(value.data(), value.size())
              .append(">\n");
          return true;
        }));

    EXPECT_EQ(result,
              "<:host> = <SomeHost>\n"
              "<key> = <val1,val2val2,val2,val3>\n"
              "<key> = <val4val5val6>\n"
              "<key> = <val11 val12>\n"
              "<key> = <v val13>\n"
              "<key> = <val7>\n"
              "<key> = <>\n"
              "<key> = <val8 , val9 ,, val10>\n"
              "<key> = < val14 >\n"
              "<key2> = <val15>\n"
              "<key> = <Val16>\n"
              "<key> = <foo, Val17, bar>\n"
              "<date> = <2 Jan 1970>\n"
              "<AcceptEncoding> = <MyFavoriteEncoding>\n");
  }

  {
    std::string result;
    EXPECT_FALSE(headers.ForEachHeader(
        [&result](const absl::string_view key, absl::string_view value) {
          result.append("<")
              .append(key.data(), key.size())
              .append("> = <")
              .append(value.data(), value.size())
              .append(">\n");
          return !value.empty();
        }));

    EXPECT_EQ(result,
              "<:host> = <SomeHost>\n"
              "<key> = <val1,val2val2,val2,val3>\n"
              "<key> = <val4val5val6>\n"
              "<key> = <val11 val12>\n"
              "<key> = <v val13>\n"
              "<key> = <val7>\n"
              "<key> = <>\n");
  }
}

TEST(BalsaHeaders, WriteToBufferWithLowerCasedHeaderKey) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("Key1", "value1");
  headers.AppendHeader("Key2", "value2");
  std::string expected_lower_case =
      "GET / HTTP/1.0\r\n"
      "key1: value1\r\n"
      "key2: value2\r\n";
  std::string expected_lower_case_with_end =
      "GET / HTTP/1.0\r\n"
      "key1: value1\r\n"
      "key2: value2\r\n\r\n";
  std::string expected_upper_case =
      "GET / HTTP/1.0\r\n"
      "Key1: value1\r\n"
      "Key2: value2\r\n";
  std::string expected_upper_case_with_end =
      "GET / HTTP/1.0\r\n"
      "Key1: value1\r\n"
      "Key2: value2\r\n\r\n";

  SimpleBuffer simple_buffer;
  headers.WriteToBuffer(&simple_buffer, BalsaHeaders::CaseOption::kLowercase,
                        BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected_lower_case));

  simple_buffer.Clear();
  headers.WriteToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(), StrEq(expected_upper_case));

  simple_buffer.Clear();
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_THAT(simple_buffer.GetReadableRegion(),
              StrEq(expected_upper_case_with_end));

  simple_buffer.Clear();
  headers.WriteHeaderAndEndingToBuffer(
      &simple_buffer, BalsaHeaders::CaseOption::kLowercase,
      BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_THAT(simple_buffer.GetReadableRegion(),
              StrEq(expected_lower_case_with_end));
}

TEST(BalsaHeaders, WriteToBufferWithProperCasedHeaderKey) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("Te", "value1");
  headers.AppendHeader("my-Test-header", "value2");
  std::string expected_proper_case =
      "GET / HTTP/1.0\r\n"
      "TE: value1\r\n"
      "My-Test-Header: value2\r\n";
  std::string expected_proper_case_with_end =
      "GET / HTTP/1.0\r\n"
      "TE: value1\r\n"
      "My-Test-Header: value2\r\n\r\n";
  std::string expected_unmodified =
      "GET / HTTP/1.0\r\n"
      "Te: value1\r\n"
      "my-Test-header: value2\r\n";
  std::string expected_unmodified_with_end =
      "GET / HTTP/1.0\r\n"
      "Te: value1\r\n"
      "my-Test-header: value2\r\n\r\n";

  SimpleBuffer simple_buffer;
  headers.WriteToBuffer(&simple_buffer, BalsaHeaders::CaseOption::kPropercase,
                        BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_proper_case);

  simple_buffer.Clear();
  headers.WriteToBuffer(&simple_buffer,
                        BalsaHeaders::CaseOption::kNoModification,
                        BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_unmodified);

  simple_buffer.Clear();
  headers.WriteHeaderAndEndingToBuffer(
      &simple_buffer, BalsaHeaders::CaseOption::kNoModification,
      BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_unmodified_with_end);

  simple_buffer.Clear();
  headers.WriteHeaderAndEndingToBuffer(
      &simple_buffer, BalsaHeaders::CaseOption::kPropercase,
      BalsaHeaders::CoalesceOption::kNoCoalesce);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_proper_case_with_end);
}

TEST(BalsaHeadersTest, ToPropercaseTest) {
  EXPECT_EQ(BalsaHeaders::ToPropercase(""), "");
  EXPECT_EQ(BalsaHeaders::ToPropercase("Foo"), "Foo");
  EXPECT_EQ(BalsaHeaders::ToPropercase("foO"), "Foo");
  EXPECT_EQ(BalsaHeaders::ToPropercase("my-test-header"), "My-Test-Header");
  EXPECT_EQ(BalsaHeaders::ToPropercase("my--test-header"), "My--Test-Header");
}

TEST(BalsaHeaders, WriteToBufferCoalescingMultivaluedHeaders) {
  BalsaHeaders::MultivaluedHeadersSet multivalued_headers;
  multivalued_headers.insert("KeY1");
  multivalued_headers.insert("another_KEY");

  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("Key1", "value1");
  headers.AppendHeader("Key2", "value2");
  headers.AppendHeader("Key1", "value11");
  headers.AppendHeader("Key2", "value21");
  headers.AppendHeader("Key1", "multiples, values, already");
  std::string expected_non_coalesced =
      "GET / HTTP/1.0\r\n"
      "Key1: value1\r\n"
      "Key2: value2\r\n"
      "Key1: value11\r\n"
      "Key2: value21\r\n"
      "Key1: multiples, values, already\r\n";
  std::string expected_coalesced =
      "Key1: value1,value11,multiples, values, already\r\n"
      "Key2: value2\r\n"
      "Key2: value21\r\n";

  SimpleBuffer simple_buffer;
  headers.WriteToBuffer(&simple_buffer);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_non_coalesced);

  simple_buffer.Clear();
  headers.WriteToBufferCoalescingMultivaluedHeaders(
      &simple_buffer, multivalued_headers,
      BalsaHeaders::CaseOption::kNoModification);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_coalesced);
}

TEST(BalsaHeaders, WriteToBufferCoalescingMultivaluedHeadersMultiLine) {
  BalsaHeaders::MultivaluedHeadersSet multivalued_headers;
  multivalued_headers.insert("Key 2");
  multivalued_headers.insert("key\n 3");

  BalsaHeaders headers;
  headers.AppendHeader("key1", "value1");
  headers.AppendHeader("key 2", "value\n 2");
  headers.AppendHeader("key\n 3", "value3");
  headers.AppendHeader("key 2", "value 21");
  headers.AppendHeader("key 3", "value 33");
  std::string expected_non_coalesced =
      "\r\n"
      "key1: value1\r\n"
      "key 2: value\n"
      " 2\r\n"
      "key\n"
      " 3: value3\r\n"
      "key 2: value 21\r\n"
      "key 3: value 33\r\n";

  SimpleBuffer simple_buffer;
  headers.WriteToBuffer(&simple_buffer);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_non_coalesced);

  std::string expected_coalesced =
      "key1: value1\r\n"
      "key 2: value\n"
      " 2,value 21\r\n"
      "key\n"
      " 3: value3\r\n"
      "key 3: value 33\r\n";

  simple_buffer.Clear();
  headers.WriteToBufferCoalescingMultivaluedHeaders(
      &simple_buffer, multivalued_headers,
      BalsaHeaders::CaseOption::kNoModification);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_coalesced);
}

TEST(BalsaHeaders, WriteToBufferCoalescingEnvoyHeaders) {
  BalsaHeaders headers;
  headers.SetRequestFirstlineFromStringPieces("GET", "/", "HTTP/1.0");
  headers.AppendHeader("User-Agent", "UserAgent1");
  headers.AppendHeader("Key2", "value2");
  headers.AppendHeader("USER-AGENT", "UA2");
  headers.AppendHeader("Set-Cookie", "Cookie1=aaa");
  headers.AppendHeader("user-agent", "agent3");
  headers.AppendHeader("Set-Cookie", "Cookie2=bbb");
  std::string expected_non_coalesced =
      "GET / HTTP/1.0\r\n"
      "User-Agent: UserAgent1\r\n"
      "Key2: value2\r\n"
      "USER-AGENT: UA2\r\n"
      "Set-Cookie: Cookie1=aaa\r\n"
      "user-agent: agent3\r\n"
      "Set-Cookie: Cookie2=bbb\r\n"
      "\r\n";
  std::string expected_coalesced =
      "GET / HTTP/1.0\r\n"
      "User-Agent: UserAgent1,UA2,agent3\r\n"
      "Key2: value2\r\n"
      "Set-Cookie: Cookie1=aaa\r\n"
      "Set-Cookie: Cookie2=bbb\r\n"
      "\r\n";

  SimpleBuffer simple_buffer;
  headers.WriteHeaderAndEndingToBuffer(&simple_buffer);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_non_coalesced);

  simple_buffer.Clear();
  headers.WriteHeaderAndEndingToBuffer(
      &simple_buffer, BalsaHeaders::CaseOption::kNoModification,
      BalsaHeaders::CoalesceOption::kCoalesce);
  EXPECT_EQ(simple_buffer.GetReadableRegion(), expected_coalesced);
}

TEST(BalsaHeadersTest, RemoveLastTokenFromOneLineHeader) {
  BalsaHeaders headers =
      CreateHTTPHeaders(true,
                        "GET /foo HTTP/1.1\r\n"
                        "Content-Length: 0\r\n"
                        "Content-Encoding: gzip, 3des, tar, prc\r\n\r\n");

  BalsaHeaders::const_header_lines_key_iterator it =
      headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des, tar, prc", it->second);
  EXPECT_EQ(headers.header_lines_key_end(), ++it);

  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des, tar", it->second);
  EXPECT_EQ(headers.header_lines_key_end(), ++it);

  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des", it->second);
  EXPECT_EQ(headers.header_lines_key_end(), ++it);

  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip", it->second);
  EXPECT_EQ(headers.header_lines_key_end(), ++it);

  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");

  EXPECT_FALSE(headers.HasHeader("Content-Encoding"));
}

TEST(BalsaHeadersTest, RemoveLastTokenFromMultiLineHeader) {
  BalsaHeaders headers =
      CreateHTTPHeaders(true,
                        "GET /foo HTTP/1.1\r\n"
                        "Content-Length: 0\r\n"
                        "Content-Encoding: gzip, 3des\r\n"
                        "Content-Encoding: tar, prc\r\n\r\n");

  BalsaHeaders::const_header_lines_key_iterator it =
      headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des", it->second);
  ASSERT_EQ("tar, prc", (++it)->second);
  ASSERT_EQ(headers.header_lines_key_end(), ++it);

  // First, we should start removing tokens from the second line.
  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des", it->second);
  ASSERT_EQ("tar", (++it)->second);
  ASSERT_EQ(headers.header_lines_key_end(), ++it);

  // Second line should be entirely removed after all its tokens are gone.
  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip, 3des", it->second);
  ASSERT_EQ(headers.header_lines_key_end(), ++it);

  // Now we should be removing the tokens from the first line.
  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  it = headers.GetIteratorForKey("Content-Encoding");
  ASSERT_EQ("gzip", it->second);
  ASSERT_EQ(headers.header_lines_key_end(), ++it);

  headers.RemoveLastTokenFromHeaderValue("Content-Encoding");
  EXPECT_FALSE(headers.HasHeader("Content-Encoding"));
}

TEST(BalsaHeadersTest, ResponseCanHaveBody) {
  // 1xx, 204 no content and 304 not modified responses can't have bodies.
  EXPECT_FALSE(BalsaHeaders::ResponseCanHaveBody(100));
  EXPECT_FALSE(BalsaHeaders::ResponseCanHaveBody(101));
  EXPECT_FALSE(BalsaHeaders::ResponseCanHaveBody(102));
  EXPECT_FALSE(BalsaHeaders::ResponseCanHaveBody(204));
  EXPECT_FALSE(BalsaHeaders::ResponseCanHaveBody(304));

  // Other responses can have body.
  EXPECT_TRUE(BalsaHeaders::ResponseCanHaveBody(200));
  EXPECT_TRUE(BalsaHeaders::ResponseCanHaveBody(302));
  EXPECT_TRUE(BalsaHeaders::ResponseCanHaveBody(404));
  EXPECT_TRUE(BalsaHeaders::ResponseCanHaveBody(502));
}

}  // namespace

}  // namespace test

}  // namespace quiche
