// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/qpack/qpack_encoder.h"

#include <limits>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/qpack/qpack_instruction_encoder.h"
#include "quiche/quic/core/qpack/value_splitting_header_list.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/qpack/qpack_encoder_peer.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrictMock;

namespace quic {
namespace test {
namespace {

// A number larger than kMaxBytesBufferedByStream in
// qpack_encoder_stream_sender.cc.  Returning this value from NumBytesBuffered()
// will instruct QpackEncoder not to generate any instructions for the encoder
// stream.
constexpr uint64_t kTooManyBytesBuffered = 1024 * 1024;

std::string PrintToString(const testing::TestParamInfo<HuffmanEncoding>& info) {
  switch (info.param) {
    case HuffmanEncoding::kEnabled:
      return "HuffmanEnabled";
    case HuffmanEncoding::kDisabled:
      return "HuffmanDisabled";
  }

  QUICHE_NOTREACHED();
  return "InvalidValue";
}

// Mock QpackEncoder::DecoderStreamErrorDelegate implementation.
class MockDecoderStreamErrorDelegate
    : public QpackEncoder::DecoderStreamErrorDelegate {
 public:
  ~MockDecoderStreamErrorDelegate() override = default;

  MOCK_METHOD(void, OnDecoderStreamError,
              (QuicErrorCode error_code, absl::string_view error_message),
              (override));
};

class QpackEncoderTest : public QuicTestWithParam<HuffmanEncoding> {
 protected:
  QpackEncoderTest()
      : huffman_encoding_(GetParam()),
        encoder_(&decoder_stream_error_delegate_, huffman_encoding_,
                 CookieCrumbling::kEnabled),
        encoder_stream_sent_byte_count_(0) {
    encoder_.set_qpack_stream_sender_delegate(&encoder_stream_sender_delegate_);
    encoder_.SetMaximumBlockedStreams(1);
  }

  ~QpackEncoderTest() override = default;

  bool HuffmanEnabled() const {
    return huffman_encoding_ == HuffmanEncoding::kEnabled;
  }

  std::string Encode(const quiche::HttpHeaderBlock& header_list) {
    return encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list,
                                     &encoder_stream_sent_byte_count_);
  }

  const HuffmanEncoding huffman_encoding_;
  StrictMock<MockDecoderStreamErrorDelegate> decoder_stream_error_delegate_;
  StrictMock<MockQpackStreamSenderDelegate> encoder_stream_sender_delegate_;
  QpackEncoder encoder_;
  QuicByteCount encoder_stream_sent_byte_count_;
};

INSTANTIATE_TEST_SUITE_P(HuffmanEncoding, QpackEncoderTest,
                         ::testing::ValuesIn({HuffmanEncoding::kEnabled,
                                              HuffmanEncoding::kDisabled}),
                         PrintToString);

TEST_P(QpackEncoderTest, Empty) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  std::string output = Encode(header_list);

  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes("0000", &expected_output));
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, EmptyName) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list[""] = "foo";
  std::string output = Encode(header_list);

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes("0000208294e7", &expected_output));
  } else {
    ASSERT_TRUE(absl::HexStringToBytes("00002003666f6f", &expected_output));
  }
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, EmptyValue) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = "";
  std::string output = Encode(header_list);

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes("00002a94e700", &expected_output));
  } else {
    ASSERT_TRUE(absl::HexStringToBytes("000023666f6f00", &expected_output));
  }
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, EmptyNameAndValue) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list[""] = "";
  std::string output = Encode(header_list);

  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes("00002000", &expected_output));
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, Simple) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = "bar";
  std::string output = Encode(header_list);

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes("00002a94e703626172", &expected_output));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("000023666f6f03626172", &expected_output));
  }
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, Multiple) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = "bar";
  // 'Z' would be Huffman encoded to 8 bits, so no Huffman encoding is used.
  header_list["ZZZZZZZ"] = std::string(127, 'Z');
  std::string output = Encode(header_list);

  std::string expected_output_hex;
  if (HuffmanEnabled()) {
    expected_output_hex =
        "0000"             // prefix
        "2a94e703626172";  // foo: bar
  } else {
    expected_output_hex =
        "0000"               // prefix
        "23666f6f03626172";  // foo: bar
  }
  expected_output_hex +=
      "27005a5a5a5a5a5a5a"  // 7 octet long header name, the smallest number
                            // that does not fit on a 3-bit prefix.
      "7f005a5a5a5a5a5a5a"  // 127 octet long header value, the smallest
      "5a5a5a5a5a5a5a5a5a"  // number that does not fit on a 7-bit prefix.
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a"
      "5a5a5a5a5a5a5a5a5a";
  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes(expected_output_hex, &expected_output));
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, StaticTable) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  {
    quiche::HttpHeaderBlock header_list;
    header_list[":method"] = "GET";
    header_list["accept-encoding"] = "gzip, deflate, br";
    header_list["location"] = "";

    std::string output = Encode(header_list);
    std::string expected_output;
    ASSERT_TRUE(absl::HexStringToBytes("0000d1dfcc", &expected_output));
    EXPECT_EQ(expected_output, output);
  }
  {
    quiche::HttpHeaderBlock header_list;
    header_list[":method"] = "POST";
    header_list["accept-encoding"] = "compress";
    header_list["location"] = "foo";

    std::string output = Encode(header_list);
    std::string expected_output;
    if (HuffmanEnabled()) {
      ASSERT_TRUE(absl::HexStringToBytes("0000d45f108621e9aec2a11f5c8294e7",
                                         &expected_output));
    } else {
      ASSERT_TRUE(absl::HexStringToBytes(
          "0000d45f1008636f6d70726573735c03666f6f", &expected_output));
    }
    EXPECT_EQ(expected_output, output);
  }
  {
    quiche::HttpHeaderBlock header_list;
    header_list[":method"] = "TRACE";
    header_list["accept-encoding"] = "";

    std::string output = Encode(header_list);
    std::string expected_output;
    ASSERT_TRUE(
        absl::HexStringToBytes("00005f000554524143455f1000", &expected_output));
    EXPECT_EQ(expected_output, output);
  }
}

TEST_P(QpackEncoderTest, DecoderStreamError) {
  EXPECT_CALL(decoder_stream_error_delegate_,
              OnDecoderStreamError(QUIC_QPACK_DECODER_STREAM_INTEGER_TOO_LARGE,
                                   Eq("Encoded integer too large.")));

  QpackEncoder encoder(&decoder_stream_error_delegate_, huffman_encoding_,
                       CookieCrumbling::kEnabled);
  encoder.set_qpack_stream_sender_delegate(&encoder_stream_sender_delegate_);
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes("ffffffffffffffffffffff", &input));
  encoder.decoder_stream_receiver()->Decode(input);
}

TEST_P(QpackEncoderTest, SplitAlongNullCharacter) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = absl::string_view("bar\0bar\0baz", 11);
  std::string output = Encode(header_list);

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"             // prefix
                               "2a94e703626172"   // foo: bar
                               "2a94e703626172"   // foo: bar
                               "2a94e70362617a",  // foo: baz
                               &expected_output));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"               // prefix
                               "23666f6f03626172"   // foo: bar
                               "23666f6f03626172"   // foo: bar
                               "23666f6f0362617a",  // foo: bar
                               &expected_output));
  }
  EXPECT_EQ(expected_output, output);
}

TEST_P(QpackEncoderTest, ZeroInsertCountIncrement) {
  // Encoder receives insert count increment with forbidden value 0.
  EXPECT_CALL(
      decoder_stream_error_delegate_,
      OnDecoderStreamError(QUIC_QPACK_DECODER_STREAM_INVALID_ZERO_INCREMENT,
                           Eq("Invalid increment value 0.")));
  encoder_.OnInsertCountIncrement(0);
}

TEST_P(QpackEncoderTest, TooLargeInsertCountIncrement) {
  // Encoder receives insert count increment with value that increases Known
  // Received Count to a value (one) which is larger than the number of dynamic
  // table insertions sent (zero).
  EXPECT_CALL(
      decoder_stream_error_delegate_,
      OnDecoderStreamError(QUIC_QPACK_DECODER_STREAM_IMPOSSIBLE_INSERT_COUNT,
                           Eq("Increment value 1 raises known received count "
                              "to 1 exceeding inserted entry count 0")));
  encoder_.OnInsertCountIncrement(1);
}

// Regression test for https://crbug.com/1014372.
TEST_P(QpackEncoderTest, InsertCountIncrementOverflow) {
  QpackEncoderHeaderTable* header_table =
      QpackEncoderPeer::header_table(&encoder_);

  // Set dynamic table capacity large enough to hold one entry.
  header_table->SetMaximumDynamicTableCapacity(4096);
  header_table->SetDynamicTableCapacity(4096);
  // Insert one entry into the header table.
  header_table->InsertEntry("foo", "bar");

  // Receive Insert Count Increment instruction with increment value 1.
  encoder_.OnInsertCountIncrement(1);

  // Receive Insert Count Increment instruction that overflows the known
  // received count.  This must result in an error instead of a crash.
  EXPECT_CALL(decoder_stream_error_delegate_,
              OnDecoderStreamError(
                  QUIC_QPACK_DECODER_STREAM_INCREMENT_OVERFLOW,
                  Eq("Insert Count Increment instruction causes overflow.")));
  encoder_.OnInsertCountIncrement(std::numeric_limits<uint64_t>::max());
}

TEST_P(QpackEncoderTest, InvalidHeaderAcknowledgement) {
  // Encoder receives header acknowledgement for a stream on which no header
  // block with dynamic table entries was ever sent.
  EXPECT_CALL(
      decoder_stream_error_delegate_,
      OnDecoderStreamError(QUIC_QPACK_DECODER_STREAM_INCORRECT_ACKNOWLEDGEMENT,
                           Eq("Header Acknowledgement received for stream 0 "
                              "with no outstanding header blocks.")));
  encoder_.OnHeaderAcknowledgement(/* stream_id = */ 0);
}

TEST_P(QpackEncoderTest, DynamicTable) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list.AppendValueOrAddHeader("foo",
                                     "baz");  // name matches dynamic entry
  header_list["cookie"] = "baz";              // name matches static entry

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));
  // Insert three entries into the dynamic table.
  std::string insert_entries_hex;
  if (HuffmanEnabled()) {
    insert_entries_hex =
        "62"     // insert without name reference
        "94e7";  // Huffman-encoded literal name "foo"
  } else {
    insert_entries_hex =
        "43"       // insert without name reference
        "666f6f";  // literal name "foo"
  }
  insert_entries_hex +=
      "03626172"   // value "bar"
      "80"         // insert with name reference, dynamic index 0
      "0362617a"   // value "baz"
      "c5"         // insert with name reference, static index 5
      "0362617a";  // value "baz"
  std::string insert_entries;
  ASSERT_TRUE(absl::HexStringToBytes(insert_entries_hex, &insert_entries));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries))));

  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes(
      "0400"     // prefix
      "828180",  // dynamic entries with relative index 0, 1, and 2
      &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list));

  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);
}

// There is no room in the dynamic table after inserting the first entry.
TEST_P(QpackEncoderTest, SmallDynamicTable) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(QpackEntry::Size("foo", "bar"));
  encoder_.SetDynamicTableCapacity(QpackEntry::Size("foo", "bar"));

  quiche::HttpHeaderBlock header_list;
  header_list["foo"] = "bar";
  header_list.AppendValueOrAddHeader("foo",
                                     "baz");  // name matches dynamic entry
  header_list["cookie"] = "baz";              // name matches static entry
  header_list["bar"] = "baz";                 // no match

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3f07", &set_dyanamic_table_capacity));
  // Insert one entry into the dynamic table.
  std::string insert_entry;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("62"    // insert without name reference
                               "94e7"  // Huffman-encoded literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entry));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("43"         // insert without name reference
                               "666f6f"     // literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entry));
  }
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(
                  Eq(absl::StrCat(set_dyanamic_table_capacity, insert_entry))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0200"       // prefix
                             "80"         // dynamic entry 0
                             "40"         // reference to dynamic entry 0 name
                             "0362617a"   // with literal value "baz"
                             "55"         // reference to static entry 5 name
                             "0362617a"   // with literal value "baz"
                             "23626172"   // literal name "bar"
                             "0362617a",  // with literal value "baz"
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list));

  EXPECT_EQ(insert_entry.size(), encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, BlockedStream) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list1;
  header_list1["foo"] = "bar";

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));
  // Insert one entry into the dynamic table.
  std::string insert_entry1;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("62"    // insert without name reference
                               "94e7"  // Huffman-encoded literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entry1));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("43"         // insert without name reference
                               "666f6f"     // literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entry1));
  }
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entry1))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0200"  // prefix
                             "80",   // dynamic entry 0
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list1,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(insert_entry1.size(), encoder_stream_sent_byte_count_);

  // Stream 1 is blocked.  Stream 2 is not allowed to block.
  quiche::HttpHeaderBlock header_list2;
  header_list2["foo"] = "bar";  // name and value match dynamic entry
  header_list2.AppendValueOrAddHeader("foo",
                                      "baz");  // name matches dynamic entry
  header_list2["cookie"] = "baz";              // name matches static entry
  header_list2["bar"] = "baz";                 // no match

  std::string entries;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "2a94e7"     // literal name "foo"
                               "03626172"   // with literal value "bar"
                               "2a94e7"     // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a"   // with literal value "baz"
                               "23626172"   // literal name "bar"
                               "0362617a",  // with literal value "baz"
                               &entries));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "23666f6f"   // literal name "foo"
                               "03626172"   // with literal value "bar"
                               "23666f6f"   // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a"   // with literal value "baz"
                               "23626172"   // literal name "bar"
                               "0362617a",  // with literal value "baz"
                               &entries));
  }
  EXPECT_EQ(entries,
            encoder_.EncodeHeaderList(/* stream_id = */ 2, header_list2,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);

  // Peer acknowledges receipt of one dynamic table entry.
  // Stream 1 is no longer blocked.
  encoder_.OnInsertCountIncrement(1);

  // Insert three entries into the dynamic table.
  std::string insert_entries;
  ASSERT_TRUE(absl::HexStringToBytes(
      "80"         // insert with name reference, dynamic index 0
      "0362617a"   // value "baz"
      "c5"         // insert with name reference, static index 5
      "0362617a"   // value "baz"
      "43"         // insert without name reference
      "626172"     // name "bar"
      "0362617a",  // value "baz"
      &insert_entries));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(insert_entries)));

  ASSERT_TRUE(
      absl::HexStringToBytes("0500"       // prefix
                             "83828180",  // dynamic entries
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 3, header_list2,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);

  // Stream 3 is blocked.  Stream 4 is not allowed to block, but it can
  // reference already acknowledged dynamic entry 0.
  std::string expected2;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0200"       // prefix
                               "80"         // dynamic entry 0
                               "2a94e7"     // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a"   // with literal value "baz"
                               "23626172"   // literal name "bar"
                               "0362617a",  // with literal value "baz"
                               &expected2));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0200"       // prefix
                               "80"         // dynamic entry 0
                               "23666f6f"   // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a"   // with literal value "baz"
                               "23626172"   // literal name "bar"
                               "0362617a",  // with literal value "baz"
                               &expected2));
  }
  EXPECT_EQ(expected2,
            encoder_.EncodeHeaderList(/* stream_id = */ 4, header_list2,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);

  // Peer acknowledges receipt of two more dynamic table entries.
  // Stream 3 is still blocked.
  encoder_.OnInsertCountIncrement(2);

  // Stream 5 is not allowed to block, but it can reference already acknowledged
  // dynamic entries 0, 1, and 2.
  std::string expected3;
  ASSERT_TRUE(
      absl::HexStringToBytes("0400"       // prefix
                             "828180"     // dynamic entries
                             "23626172"   // literal name "bar"
                             "0362617a",  // with literal value "baz"
                             &expected3));
  EXPECT_EQ(expected3,
            encoder_.EncodeHeaderList(/* stream_id = */ 5, header_list2,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);

  // Peer acknowledges decoding header block on stream 3.
  // Stream 3 is not blocked any longer.
  encoder_.OnHeaderAcknowledgement(3);

  std::string expected4;
  ASSERT_TRUE(
      absl::HexStringToBytes("0500"       // prefix
                             "83828180",  // dynamic entries
                             &expected4));
  EXPECT_EQ(expected4,
            encoder_.EncodeHeaderList(/* stream_id = */ 6, header_list2,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, Draining) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list1;
  header_list1["one"] = "foo";
  header_list1["two"] = "foo";
  header_list1["three"] = "foo";
  header_list1["four"] = "foo";
  header_list1["five"] = "foo";
  header_list1["six"] = "foo";
  header_list1["seven"] = "foo";
  header_list1["eight"] = "foo";
  header_list1["nine"] = "foo";
  header_list1["ten"] = "foo";

  // Make just enough room in the dynamic table for the header list plus the
  // first entry duplicated.  This will ensure that the oldest entries are
  // draining.
  uint64_t maximum_dynamic_table_capacity = 0;
  for (const auto& header_field : header_list1) {
    maximum_dynamic_table_capacity +=
        QpackEntry::Size(header_field.first, header_field.second);
  }
  maximum_dynamic_table_capacity += QpackEntry::Size("one", "foo");
  encoder_.SetMaximumDynamicTableCapacity(maximum_dynamic_table_capacity);
  encoder_.SetDynamicTableCapacity(maximum_dynamic_table_capacity);

  // Set Dynamic Table Capacity instruction and insert ten entries into the
  // dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_, WriteStreamData(_));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0b00"                   // prefix
                             "89888786858483828180",  // dynamic entries
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list1));

  // Entry is identical to oldest one, which is draining.  It will be
  // duplicated and referenced.
  quiche::HttpHeaderBlock header_list2;
  header_list2["one"] = "foo";

  // Duplicate oldest entry.
  ASSERT_TRUE(absl::HexStringToBytes("09", &expected_output));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(expected_output)));

  ASSERT_TRUE(
      absl::HexStringToBytes("0c00"  // prefix
                             "80",   // most recent dynamic table entry
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list2));

  quiche::HttpHeaderBlock header_list3;
  // Entry is identical to second oldest one, which is draining.  There is no
  // room to duplicate, it will be encoded with string literals.
  header_list3.AppendValueOrAddHeader("two", "foo");
  // Entry has name identical to second oldest one, which is draining.  There is
  // no room to insert new entry, it will be encoded with string literals.
  header_list3.AppendValueOrAddHeader("two", "bar");

  std::string entries =
      "0000"       // prefix
      "2374776f";  // literal name "two"
  if (HuffmanEnabled()) {
    entries += "8294e7";  // literal value "foo"
  } else {
    entries += "03666f6f";  // literal name "foo"
  }
  entries +=
      "2374776f"   // literal name "two"
      "03626172";  // literal value "bar"
  ASSERT_TRUE(absl::HexStringToBytes(entries, &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list3));
}

TEST_P(QpackEncoderTest, DynamicTableCapacityLessThanMaximum) {
  encoder_.SetMaximumDynamicTableCapacity(1024);
  encoder_.SetDynamicTableCapacity(30);

  QpackEncoderHeaderTable* header_table =
      QpackEncoderPeer::header_table(&encoder_);

  EXPECT_EQ(1024u, header_table->maximum_dynamic_table_capacity());
  EXPECT_EQ(30u, header_table->dynamic_table_capacity());
}

TEST_P(QpackEncoderTest, EncoderStreamWritesDisallowedThenAllowed) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(kTooManyBytesBuffered));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list1;
  header_list1["foo"] = "bar";
  header_list1.AppendValueOrAddHeader("foo", "baz");
  header_list1["cookie"] = "baz";  // name matches static entry

  // Encoder is not allowed to write on the encoder stream.
  // No Set Dynamic Table Capacity or Insert instructions are sent.
  // Headers are encoded as string literals.
  std::string entries;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "2a94e7"     // literal name "foo"
                               "03626172"   // with literal value "bar"
                               "2a94e7"     // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a",  // with literal value "baz"
                               &entries));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "23666f6f"   // literal name "foo"
                               "03626172"   // with literal value "bar"
                               "23666f6f"   // literal name "foo"
                               "0362617a"   // with literal value "baz"
                               "55"         // name of static entry 5
                               "0362617a",  // with literal value "baz"
                               &entries));
  }
  EXPECT_EQ(entries, Encode(header_list1));

  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);

  // If number of bytes buffered by encoder stream goes under the threshold,
  // then QpackEncoder will resume emitting encoder stream instructions.
  ::testing::Mock::VerifyAndClearExpectations(&encoder_stream_sender_delegate_);
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));

  quiche::HttpHeaderBlock header_list2;
  header_list2["foo"] = "bar";
  header_list2.AppendValueOrAddHeader("foo",
                                      "baz");  // name matches dynamic entry
  header_list2["cookie"] = "baz";              // name matches static entry

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));
  // Insert three entries into the dynamic table.
  std::string insert_entries_hex;
  if (HuffmanEnabled()) {
    insert_entries_hex =
        "62"     // insert without name reference
        "94e7";  // Huffman-encoded literal name "foo"
  } else {
    insert_entries_hex =
        "43"       // insert without name reference
        "666f6f";  // literal name "foo"
  }
  insert_entries_hex +=
      "03626172"   // value "bar"
      "80"         // insert with name reference, dynamic index 0
      "0362617a"   // value "baz"
      "c5"         // insert with name reference, static index 5
      "0362617a";  // value "baz"
  std::string insert_entries;
  ASSERT_TRUE(absl::HexStringToBytes(insert_entries_hex, &insert_entries));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries))));

  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes(
      "0400"     // prefix
      "828180",  // dynamic entries with relative index 0, 1, and 2
      &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list2));

  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, EncoderStreamWritesAllowedThenDisallowed) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list1;
  header_list1["foo"] = "bar";
  header_list1.AppendValueOrAddHeader("foo",
                                      "baz");  // name matches dynamic entry
  header_list1["cookie"] = "baz";              // name matches static entry

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));
  // Insert three entries into the dynamic table.
  std::string insert_entries_hex;
  if (HuffmanEnabled()) {
    insert_entries_hex =
        "62"     // insert without name reference
        "94e7";  // Huffman-encoded literal name "foo"
  } else {
    insert_entries_hex =
        "43"       // insert without name reference
        "666f6f";  // literal name "foo"
  }
  insert_entries_hex +=
      "03626172"   // value "bar"
      "80"         // insert with name reference, dynamic index 0
      "0362617a"   // value "baz"
      "c5"         // insert with name reference, static index 5
      "0362617a";  // value "baz"
  std::string insert_entries;
  ASSERT_TRUE(absl::HexStringToBytes(insert_entries_hex, &insert_entries));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries))));

  std::string expected_output;
  ASSERT_TRUE(absl::HexStringToBytes(
      "0400"     // prefix
      "828180",  // dynamic entries with relative index 0, 1, and 2
      &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list1));

  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);

  // If number of bytes buffered by encoder stream goes over the threshold,
  // then QpackEncoder will stop emitting encoder stream instructions.
  ::testing::Mock::VerifyAndClearExpectations(&encoder_stream_sender_delegate_);
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(kTooManyBytesBuffered));

  quiche::HttpHeaderBlock header_list2;
  header_list2["foo"] = "bar";  // matches previously inserted dynamic entry
  header_list2["bar"] = "baz";
  header_list2["cookie"] = "baz";  // name matches static entry

  // Encoder is not allowed to write on the encoder stream.
  // No Set Dynamic Table Capacity or Insert instructions are sent.
  // Headers are encoded as string literals.
  ASSERT_TRUE(
      absl::HexStringToBytes("0400"      // prefix
                             "82"        // dynamic entry with relative index 0
                             "23626172"  // literal name "bar"
                             "0362617a"  // with literal value "baz"
                             "80",       // dynamic entry with relative index 2
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list2));

  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);
}

// Regression test for https://crbug.com/1441880.
TEST_P(QpackEncoderTest, UnackedEntryCannotBeEvicted) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(2);
  // With 32 byte overhead per entry, only one entry fits in the dynamic table.
  encoder_.SetMaximumDynamicTableCapacity(40);
  encoder_.SetDynamicTableCapacity(40);

  QpackEncoderHeaderTable* header_table =
      QpackEncoderPeer::header_table(&encoder_);
  EXPECT_EQ(0u, header_table->inserted_entry_count());
  EXPECT_EQ(0u, header_table->dropped_entry_count());

  quiche::HttpHeaderBlock header_list1;
  header_list1["foo"] = "bar";

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3f09", &set_dyanamic_table_capacity));
  // Insert one entry into the dynamic table.
  std::string insert_entries1;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("62"    // insert without name reference
                               "94e7"  // Huffman-encoded literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entries1));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("43"         // insert without name reference
                               "666f6f"     // literal name "foo"
                               "03626172",  // value "bar"
                               &insert_entries1));
  }
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries1))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0200"  // prefix
                             "80",   // dynamic entry with relative index 0
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list1,
                                      &encoder_stream_sent_byte_count_));

  EXPECT_EQ(1u, header_table->inserted_entry_count());
  EXPECT_EQ(0u, header_table->dropped_entry_count());

  encoder_.OnStreamCancellation(/* stream_id = */ 1);

  // At this point, entry 0 has no references to it, because stream 1 is
  // cancelled.  However, this entry is unacknowledged, therefore it must not be
  // evicted according to RFC 9204 Section 2.1.1.

  quiche::HttpHeaderBlock header_list2;
  header_list2["bar"] = "baz";

  ASSERT_TRUE(
      absl::HexStringToBytes("0000"       // prefix
                             "23626172"   // literal name "bar"
                             "0362617a",  // literal value "baz"
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 2, header_list2,
                                      &encoder_stream_sent_byte_count_));

  EXPECT_EQ(1u, header_table->inserted_entry_count());
  EXPECT_EQ(0u, header_table->dropped_entry_count());
}

// Header name and value match an entry in the dynamic table, but that entry
// cannot be used. If there is an entry with matching name in the static table,
// use that.
TEST_P(QpackEncoderTest, UseStaticTableNameOnlyMatch) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(2);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list;
  header_list[":method"] = "bar";

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));

  // Insert one entry into the dynamic table.
  std::string insert_entry1;
  ASSERT_TRUE(
      absl::HexStringToBytes("cf"  // insert with name of static table entry 15
                             "03626172",  // literal value "bar"
                             &insert_entry1));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entry1))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0200"  // prefix
                             "80",   // dynamic entry 0
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 1, header_list,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(insert_entry1.size(), encoder_stream_sent_byte_count_);

  // Stream 2 uses the same dynamic entry.
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 2, header_list,
                                      &encoder_stream_sent_byte_count_));
  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);

  // Streams 1 and 2 are blocked, therefore stream 3 is not allowed to refer to
  // the existing dynamic table entry, nor to add a new entry to the dynamic
  // table.
  ASSERT_TRUE(
      absl::HexStringToBytes("0000"  // prefix
                             "5f00"  // name reference to static table entry 15
                             "03626172",  // literal value "bar"
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder_.EncodeHeaderList(/* stream_id = */ 3, header_list,
                                      &encoder_stream_sent_byte_count_));
}

// Header name and value match an entry in the dynamic table, but that entry
// cannot be used. If there is an entry with matching name in the dynamic table
// that can be used, do so.
TEST_P(QpackEncoderTest, UseDynamicTableNameOnlyMatch) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  quiche::HttpHeaderBlock header_list1;
  header_list1["one"] = "foo";
  header_list1["two"] = "foo";
  header_list1["three"] = "foo";
  header_list1["four"] = "foo";
  header_list1["five"] = "foo";
  header_list1["six"] = "foo";
  header_list1["seven"] = "foo";
  header_list1["eight"] = "foo";
  header_list1["nine"] = "foo";
  header_list1["ten"] = "foo";

  // Make just enough room in the dynamic table for the header list,
  // plus another entry using the name of the first one,
  // This will ensure that the oldest entries are draining.
  uint64_t maximum_dynamic_table_capacity = 0;
  for (const auto& header_field : header_list1) {
    maximum_dynamic_table_capacity +=
        QpackEntry::Size(header_field.first, header_field.second);
  }
  maximum_dynamic_table_capacity += QpackEntry::Size("one", "bar");
  encoder_.SetMaximumDynamicTableCapacity(maximum_dynamic_table_capacity);
  encoder_.SetDynamicTableCapacity(maximum_dynamic_table_capacity);

  // Set Dynamic Table Capacity instruction and insert ten entries into the
  // dynamic table.
  EXPECT_CALL(encoder_stream_sender_delegate_, WriteStreamData(_));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0b00"                   // prefix
                             "89888786858483828180",  // dynamic entries
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list1));

  // Entry has the same name as the first one.
  quiche::HttpHeaderBlock header_list2;
  header_list2["one"] = "bar";

  ASSERT_TRUE(absl::HexStringToBytes(
      "89"         // insert entry with same name as dynamic table entry 9
      "03626172",  // and literal value "bar"
      &expected_output));
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(expected_output)));

  ASSERT_TRUE(
      absl::HexStringToBytes("0c00"  // prefix
                             "80",   // most recent dynamic table entry
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list2));

  // Entry is identical to the first one, which is draining, and has the same
  // name but different value as the last one, which is not draining.
  quiche::HttpHeaderBlock header_list3;
  header_list3["one"] = "foo";

  // Entry matches name and value of oldest dynamic table entry, which cannot be
  // used. Use the name of the most recent dynamic table entry instead, and
  // encode value as string literal.
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0c00"     // prefix
                               "40"       // name as dynamic table entry 0
                               "8294e7",  // Huffman-encoded literal value "foo"
                               &expected_output));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0c00"       // prefix
                               "40"         // name as dynamic table entry 0
                               "03666f6f",  // literal value "foo"
                               &expected_output));
  }
  EXPECT_EQ(expected_output, Encode(header_list3));
}

TEST_P(QpackEncoderTest, CookieCrumblingEnabledNoDynamicTable) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));

  quiche::HttpHeaderBlock header_list;
  header_list["cookie"] = "foo; bar";

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "55"         // name of static entry 5
                               "8294e7"     // with literal value "bar"
                               "55"         // name of static entry 5
                               "03626172",  // with literal value "bar"
                               &expected_output));
  } else {
    ASSERT_TRUE(
        absl::HexStringToBytes("0000"       // prefix
                               "55"         // name of static entry 5
                               "03666f6f"   // with literal value "foo"
                               "55"         // name of static entry 5
                               "03626172",  // with literal value "bar"
                               &expected_output));
  }
  EXPECT_EQ(expected_output, Encode(header_list));

  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, CookieCrumblingEnabledDynamicTable) {
  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder_.SetMaximumBlockedStreams(1);
  encoder_.SetMaximumDynamicTableCapacity(4096);
  encoder_.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list;
  header_list["cookie"] = "foo; bar";

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));

  // Insert entries into the dynamic table.
  std::string insert_entries;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes(
        "c5"         // insert with name reference, static index 5
        "8294e7"     // with literal value "foo"
        "c5"         // insert with name reference, static index 5
        "03626172",  // with literal value "bar"
        &insert_entries));
  } else {
    ASSERT_TRUE(absl::HexStringToBytes(
        "c5"         // insert with name reference, static index 5
        "03666f6f"   // with literal value "foo"
        "c5"         // insert with name reference, static index 5
        "03626172",  // with literal value "bar"
        &insert_entries));
  }
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0300"  // prefix
                             "81"    // dynamic entry with relative index 0
                             "80",   // dynamic entry with relative index 1
                             &expected_output));
  EXPECT_EQ(expected_output, Encode(header_list));

  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, CookieCrumblingDisabledNoDynamicTable) {
  QpackEncoder encoder(&decoder_stream_error_delegate_, huffman_encoding_,
                       CookieCrumbling::kDisabled);

  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));

  quiche::HttpHeaderBlock header_list;
  header_list["cookie"] = "foo; bar";

  std::string expected_output;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes(
        "0000"             // prefix
        "55"               // name of static entry 5
        "8694e7fb5231d9",  // with literal value "foo; bar"
        &expected_output));
  } else {
    ASSERT_TRUE(absl::HexStringToBytes(
        "0000"                 // prefix
        "55"                   // name of static entry 5
        "08666f6f3b20626172",  // with literal value "foo; bar"
        &expected_output));
  }
  EXPECT_EQ(expected_output,
            encoder.EncodeHeaderList(/* stream_id = */ 1, header_list,
                                     &encoder_stream_sent_byte_count_));

  EXPECT_EQ(0u, encoder_stream_sent_byte_count_);
}

TEST_P(QpackEncoderTest, CookieCrumblingDisabledDynamicTable) {
  QpackEncoder encoder(&decoder_stream_error_delegate_, huffman_encoding_,
                       CookieCrumbling::kDisabled);
  encoder.SetMaximumBlockedStreams(1);
  encoder.set_qpack_stream_sender_delegate(&encoder_stream_sender_delegate_);

  EXPECT_CALL(encoder_stream_sender_delegate_, NumBytesBuffered())
      .WillRepeatedly(Return(0));
  encoder.SetMaximumBlockedStreams(1);
  encoder.SetMaximumDynamicTableCapacity(4096);
  encoder.SetDynamicTableCapacity(4096);

  quiche::HttpHeaderBlock header_list;
  header_list["cookie"] = "foo; bar";

  // Set Dynamic Table Capacity instruction.
  std::string set_dyanamic_table_capacity;
  ASSERT_TRUE(absl::HexStringToBytes("3fe11f", &set_dyanamic_table_capacity));

  // Insert entries into the dynamic table.
  std::string insert_entries;
  if (HuffmanEnabled()) {
    ASSERT_TRUE(absl::HexStringToBytes(
        "c5"               // insert with name reference, static index 5
        "8694e7fb5231d9",  // with literal value "foo; bar"
        &insert_entries));
  } else {
    ASSERT_TRUE(absl::HexStringToBytes(
        "c5"                   // insert with name reference, static index 5
        "08666f6f3b20626172",  // with literal value "foo; bar"
        &insert_entries));
  }
  EXPECT_CALL(encoder_stream_sender_delegate_,
              WriteStreamData(Eq(
                  absl::StrCat(set_dyanamic_table_capacity, insert_entries))));

  std::string expected_output;
  ASSERT_TRUE(
      absl::HexStringToBytes("0200"  // prefix
                             "80",   // dynamic entry with relative index 0
                             &expected_output));
  EXPECT_EQ(expected_output,
            encoder.EncodeHeaderList(/* stream_id = */ 1, header_list,
                                     &encoder_stream_sent_byte_count_));

  EXPECT_EQ(insert_entries.size(), encoder_stream_sent_byte_count_);
}

}  // namespace
}  // namespace test
}  // namespace quic
