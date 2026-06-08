// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/http_decoder.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/http/http_encoder.h"
#include "quiche/quic/core/http/http_frames.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;

namespace quic {
namespace test {

class HttpDecoderPeer {
 public:
  static uint64_t current_frame_type(HttpDecoder* decoder) {
    return decoder->current_frame_type_;
  }
};

namespace {

class HttpDecoderTest : public QuicTest {
 public:
  HttpDecoderTest() : decoder_(&visitor_) {
    ON_CALL(visitor_, OnMaxPushIdFrame()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnGoAwayFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnSettingsFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnSettingsFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFrameStart(_, _)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnDataFrameEnd()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFrameStart(_, _)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnHeadersFrameEnd()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnPriorityUpdateFrameStart(_))
        .WillByDefault(Return(true));
    ON_CALL(visitor_, OnPriorityUpdateFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnAcceptChFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnAcceptChFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnOriginFrameStart(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnOriginFrame(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnMetadataFrameStart(_, _)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnMetadataFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnMetadataFrameEnd()).WillByDefault(Return(true));
    ON_CALL(visitor_, OnUnknownFrameStart(_, _, _)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnUnknownFramePayload(_)).WillByDefault(Return(true));
    ON_CALL(visitor_, OnUnknownFrameEnd()).WillByDefault(Return(true));
  }
  ~HttpDecoderTest() override = default;

  uint64_t current_frame_type() {
    return HttpDecoderPeer::current_frame_type(&decoder_);
  }

  // Process |input| in a single call to HttpDecoder::ProcessInput().
  QuicByteCount ProcessInput(absl::string_view input) {
    return decoder_.ProcessInput(input.data(), input.size());
  }

  // Feed |input| to |decoder_| one character at a time,
  // verifying that each character gets processed.
  void ProcessInputCharByChar(absl::string_view input) {
    for (char c : input) {
      EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
    }
  }

  // Append garbage to |input|, then process it in a single call to
  // HttpDecoder::ProcessInput().  Verify that garbage is not read.
  QuicByteCount ProcessInputWithGarbageAppended(absl::string_view input) {
    std::string input_with_garbage_appended = absl::StrCat(input, "blahblah");
    QuicByteCount processed_bytes = ProcessInput(input_with_garbage_appended);

    // Guaranteed by HttpDecoder::ProcessInput() contract.
    QUICHE_DCHECK_LE(processed_bytes, input_with_garbage_appended.size());

    // Caller should set up visitor to pause decoding
    // before HttpDecoder would read garbage.
    EXPECT_LE(processed_bytes, input.size());

    return processed_bytes;
  }

  testing::StrictMock<MockHttpDecoderVisitor> visitor_;
  HttpDecoder decoder_;
};

TEST_F(HttpDecoderTest, InitialState) {
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, UnknownFrame) {
  std::unique_ptr<char[]> input;

  const QuicByteCount payload_lengths[] = {0, 14, 100};
  const uint64_t frame_types[] = {
      0x21, 0x40, 0x5f, 0x7e, 0x9d,  // some reserved frame types
      0x6f, 0x14                     // some unknown, not reserved frame types
  };

  for (auto payload_length : payload_lengths) {
    std::string data(payload_length, 'a');

    for (auto frame_type : frame_types) {
      const QuicByteCount total_length =
          QuicDataWriter::GetVarInt62Len(frame_type) +
          QuicDataWriter::GetVarInt62Len(payload_length) + payload_length;
      input = std::make_unique<char[]>(total_length);

      QuicDataWriter writer(total_length, input.get());
      writer.WriteVarInt62(frame_type);
      writer.WriteVarInt62(payload_length);
      const QuicByteCount header_length = writer.length();
      if (payload_length > 0) {
        writer.WriteStringPiece(data);
      }

      EXPECT_CALL(visitor_, OnUnknownFrameStart(frame_type, header_length,
                                                payload_length));
      if (payload_length > 0) {
        EXPECT_CALL(visitor_, OnUnknownFramePayload(Eq(data)));
      }
      EXPECT_CALL(visitor_, OnUnknownFrameEnd());

      EXPECT_EQ(total_length, decoder_.ProcessInput(input.get(), total_length));

      EXPECT_THAT(decoder_.error(), IsQuicNoError());
      ASSERT_EQ("", decoder_.error_detail());
      EXPECT_EQ(frame_type, current_frame_type());
    }
  }
}

TEST_F(HttpDecoderTest, CancelPush) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("03"   // type (CANCEL_PUSH)
                             "01"   // length
                             "01",  // Push Id
                             &input));

  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(1u, ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_ERROR));
  EXPECT_EQ("CANCEL_PUSH frame received.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PushPromiseFrame) {
  InSequence s;
  std::string push_promise_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("05"   // type (PUSH PROMISE)
                             "08"   // length
                             "1f",  // push id 31
                             &push_promise_bytes));
  std::string input = absl::StrCat(push_promise_bytes,
                                   "Headers");  // headers

  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(1u, ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_ERROR));
  EXPECT_EQ("PUSH_PROMISE frame received.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MaxPushId) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("0D"   // type (MAX_PUSH_ID)
                             "01"   // length
                             "01",  // Push Id
                             &input));

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame()).WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnMaxPushIdFrame());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, SettingsFrame) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes(
      "04"    // type (SETTINGS)
      "07"    // length
      "01"    // identifier (SETTINGS_QPACK_MAX_TABLE_CAPACITY)
      "02"    // content
      "06"    // identifier (SETTINGS_MAX_HEADER_LIST_SIZE)
      "05"    // content
      "4100"  // identifier, encoded on 2 bytes (0x40), value is 256 (0x100)
      "04",   // content
      &input));

  SettingsFrame frame;
  frame.values[1] = 2;
  frame.values[6] = 5;
  frame.values[256] = 4;

  // Visitor pauses processing.
  absl::string_view remaining_input(input);
  EXPECT_CALL(visitor_, OnSettingsFrameStart(2)).WillOnce(Return(false));
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnSettingsFrame(frame)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
  EXPECT_THAT(decoder_.decoded_frame_types(), ElementsAre(4));

  // Process the full frame.
  EXPECT_CALL(visitor_, OnSettingsFrameStart(2));
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
  EXPECT_THAT(decoder_.decoded_frame_types(), ElementsAre(4, 4));

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnSettingsFrameStart(2));
  EXPECT_CALL(visitor_, OnSettingsFrame(frame));
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
  EXPECT_THAT(decoder_.decoded_frame_types(), ElementsAre(4, 4, 4));
}

TEST_F(HttpDecoderTest, CorruptSettingsFrame) {
  const char* const kPayload =
      "\x42\x11"                           // two-byte id
      "\x80\x22\x33\x44"                   // four-byte value
      "\x58\x39"                           // two-byte id
      "\xf0\x22\x33\x44\x55\x66\x77\x88";  // eight-byte value
  struct {
    size_t payload_length;
    const char* const error_message;
  } kTestData[] = {
      {1, "Unable to read setting identifier."},
      {5, "Unable to read setting value."},
      {7, "Unable to read setting identifier."},
      {12, "Unable to read setting value."},
  };

  for (const auto& test_data : kTestData) {
    std::string input;
    input.push_back(4u);  // type SETTINGS
    input.push_back(test_data.payload_length);
    const size_t header_length = input.size();
    input.append(kPayload, test_data.payload_length);

    HttpDecoder decoder(&visitor_);
    EXPECT_CALL(visitor_, OnSettingsFrameStart(header_length));
    EXPECT_CALL(visitor_, OnError(&decoder));

    QuicByteCount processed_bytes =
        decoder.ProcessInput(input.data(), input.size());
    EXPECT_EQ(input.size(), processed_bytes);
    EXPECT_THAT(decoder.error(), IsError(QUIC_HTTP_FRAME_ERROR));
    EXPECT_EQ(test_data.error_message, decoder.error_detail());
  }
}

TEST_F(HttpDecoderTest, DuplicateSettingsIdentifier) {
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"   // type (SETTINGS)
                             "04"   // length
                             "01"   // identifier
                             "01"   // content
                             "01"   // identifier
                             "02",  // content
                             &input));

  EXPECT_CALL(visitor_, OnSettingsFrameStart(2));
  EXPECT_CALL(visitor_, OnError(&decoder_));

  EXPECT_EQ(input.size(), ProcessInput(input));

  EXPECT_THAT(decoder_.error(),
              IsError(QUIC_HTTP_DUPLICATE_SETTING_IDENTIFIER));
  EXPECT_EQ("Duplicate setting identifier.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, DataFrame) {
  InSequence s;
  std::string type_and_length_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("00"   // type (DATA)
                             "05",  // length
                             &type_and_length_bytes));
  std::string input = absl::StrCat(type_and_length_bytes,
                                   "Data!");  // data

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 5)).WillOnce(Return(false));
  absl::string_view remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("Data!")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnDataFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 5));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("Data!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 5));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("D")));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("t")));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, FrameHeaderPartialDelivery) {
  InSequence s;
  // A large input that will occupy more than 1 byte in the length field.
  std::string input(2048, 'x');
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      input.length(), quiche::SimpleBufferAllocator::Get());
  // Partially send only 1 byte of the header to process.
  EXPECT_EQ(1u, decoder_.ProcessInput(header.data(), 1));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Send the rest of the header.
  EXPECT_CALL(visitor_, OnDataFrameStart(3, input.length()));
  EXPECT_EQ(header.size() - 1,
            decoder_.ProcessInput(header.data() + 1, header.size() - 1));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Send data.
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view(input)));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(2048u, decoder_.ProcessInput(input.data(), 2048));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PartialDeliveryOfLargeFrameType) {
  // Use a reserved type that takes four bytes as a varint.
  const uint64_t frame_type = 0x1f * 0x222 + 0x21;
  const QuicByteCount payload_length = 0;
  const QuicByteCount header_length =
      QuicDataWriter::GetVarInt62Len(frame_type) +
      QuicDataWriter::GetVarInt62Len(payload_length);

  auto input = std::make_unique<char[]>(header_length);
  QuicDataWriter writer(header_length, input.get());
  writer.WriteVarInt62(frame_type);
  writer.WriteVarInt62(payload_length);

  EXPECT_CALL(visitor_,
              OnUnknownFrameStart(frame_type, header_length, payload_length));
  EXPECT_CALL(visitor_, OnUnknownFrameEnd());

  auto raw_input = input.get();
  for (uint64_t i = 0; i < header_length; ++i) {
    char c = raw_input[i];
    EXPECT_EQ(1u, decoder_.ProcessInput(&c, 1));
  }

  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
  EXPECT_EQ(frame_type, current_frame_type());
}

TEST_F(HttpDecoderTest, GoAway) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("07"   // type (GOAWAY)
                             "01"   // length
                             "01",  // ID
                             &input));

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})))
      .WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnGoAwayFrame(GoAwayFrame({1})));
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, HeadersFrame) {
  InSequence s;
  std::string type_and_length_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("01"   // type (HEADERS)
                             "07",  // length
                             &type_and_length_bytes));
  std::string input = absl::StrCat(type_and_length_bytes,
                                   "Headers");  // headers

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 7)).WillOnce(Return(false));
  absl::string_view remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("Headers")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 7));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("Headers")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 7));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("H")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("d")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("e")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("r")));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("s")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MetadataFrame) {
  InSequence s;
  std::string type_and_length_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("404d"  // 2 byte type (METADATA)
                             "08",   // length
                             &type_and_length_bytes));
  std::string input = absl::StrCat(type_and_length_bytes,
                                   "Metadata");  // headers

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnMetadataFrameStart(3, 8)).WillOnce(Return(false));
  absl::string_view remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(3u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("Metadata")))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_CALL(visitor_, OnMetadataFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnMetadataFrameStart(3, 8));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("Metadata")));
  EXPECT_CALL(visitor_, OnMetadataFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnMetadataFrameStart(3, 8));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("M")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("e")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("t")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("d")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("t")));
  EXPECT_CALL(visitor_, OnMetadataFramePayload(absl::string_view("a")));
  EXPECT_CALL(visitor_, OnMetadataFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyDataFrame) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("00"   // type (DATA)
                             "00",  // length
                             &input));

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 0)).WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));

  EXPECT_CALL(visitor_, OnDataFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 0));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 0));
  EXPECT_CALL(visitor_, OnDataFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyHeadersFrame) {
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("01"   // type (HEADERS)
                             "00",  // length
                             &input));

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 0)).WillOnce(Return(false));
  EXPECT_EQ(input.size(), ProcessInputWithGarbageAppended(input));

  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  EXPECT_EQ(0u, ProcessInputWithGarbageAppended(""));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 0));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 0));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd());
  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, GoawayWithOverlyLargePayload) {
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes(
      "07"   // type (GOAWAY)
      "10",  // length exceeding the maximum possible length for GOAWAY frame
      &input));
  // Process all data at once.
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(2u, ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_TOO_LARGE));
  EXPECT_EQ("Frame is too large.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, MaxPushIdWithOverlyLargePayload) {
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("0d"   // type (MAX_PUSH_ID)
                             "10",  // length exceeding the maximum possible
                                    // length for MAX_PUSH_ID frame
                             &input));
  // Process all data at once.
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(2u, ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_TOO_LARGE));
  EXPECT_EQ("Frame is too large.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, FrameWithOverlyLargePayload) {
  // Regression test for b/193919867: Ensure that reading frames with incredibly
  // large payload lengths does not lead to allocating unbounded memory.
  constexpr size_t max_input_length =
      /*max frame type varint length*/ sizeof(uint64_t) +
      /*max frame length varint length*/ sizeof(uint64_t) +
      /*one byte of payload*/ sizeof(uint8_t);
  char input[max_input_length];
  for (uint64_t frame_type = 0; frame_type < 1025; frame_type++) {
    ::testing::NiceMock<MockHttpDecoderVisitor> visitor;
    HttpDecoder decoder(&visitor);
    QuicDataWriter writer(max_input_length, input);
    ASSERT_TRUE(writer.WriteVarInt62(frame_type));  // frame type.
    ASSERT_TRUE(
        writer.WriteVarInt62(quiche::kVarInt62MaxValue));  // frame length.
    ASSERT_TRUE(writer.WriteUInt8(0x00));  // one byte of payload.
    EXPECT_NE(decoder.ProcessInput(input, writer.length()), 0u) << frame_type;
  }
}

TEST_F(HttpDecoderTest, MalformedSettingsFrame) {
  char input[30];
  QuicDataWriter writer(30, input);
  // Write type SETTINGS.
  writer.WriteUInt8(0x04);
  // Write length.
  writer.WriteVarInt62(2048 * 1024);

  writer.WriteStringPiece("Malformed payload");
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(5u, decoder_.ProcessInput(input, ABSL_ARRAYSIZE(input)));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_TOO_LARGE));
  EXPECT_EQ("Frame is too large.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, Http2Frame) {
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes(
      "06"   // PING in HTTP/2 but not supported in HTTP/3.
      "05"   // length
      "15",  // random payload
      &input));

  // Process the full frame.
  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(1u, ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_RECEIVE_SPDY_FRAME));
  EXPECT_EQ("HTTP/2 frame received in a HTTP/3 connection: 6",
            decoder_.error_detail());
}

TEST_F(HttpDecoderTest, HeadersPausedThenData) {
  InSequence s;
  std::string headers_type_and_length_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("01"   // type (HEADERS)
                             "07",  // length,
                             &headers_type_and_length_bytes));
  std::string headers = absl::StrCat(headers_type_and_length_bytes, "Headers");
  std::string data_type_and_length_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("00"   // type (DATA)
                             "05",  // length
                             &data_type_and_length_bytes));
  std::string data = absl::StrCat(data_type_and_length_bytes, "Data!");
  std::string input = absl::StrCat(headers, data);

  // Visitor pauses processing, maybe because header decompression is blocked.
  EXPECT_CALL(visitor_, OnHeadersFrameStart(2, 7));
  EXPECT_CALL(visitor_, OnHeadersFramePayload(absl::string_view("Headers")));
  EXPECT_CALL(visitor_, OnHeadersFrameEnd()).WillOnce(Return(false));
  absl::string_view remaining_input(input);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(9u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  // Process DATA frame.
  EXPECT_CALL(visitor_, OnDataFrameStart(2, 5));
  EXPECT_CALL(visitor_, OnDataFramePayload(absl::string_view("Data!")));
  EXPECT_CALL(visitor_, OnDataFrameEnd());

  processed_bytes = ProcessInput(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);

  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, CorruptFrame) {
  InSequence s;

  struct {
    const char* const input;
    const char* const error_message;
  } kTestData[] = {{"\x0D"   // type (MAX_PUSH_ID)
                    "\x01"   // length
                    "\x40",  // first byte of two-byte varint push id
                    "Unable to read MAX_PUSH_ID push_id."},
                   {"\x0D"  // type (MAX_PUSH_ID)
                    "\x04"  // length
                    "\x05"  // valid push id
                    "foo",  // superfluous data
                    "Superfluous data in MAX_PUSH_ID frame."},
                   {"\x07"   // type (GOAWAY)
                    "\x01"   // length
                    "\x40",  // first byte of two-byte varint stream id
                    "Unable to read GOAWAY ID."},
                   {"\x07"  // type (GOAWAY)
                    "\x04"  // length
                    "\x05"  // valid stream id
                    "foo",  // superfluous data
                    "Superfluous data in GOAWAY frame."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x01"      // length
                    "\x40",     // first byte of two-byte varint origin length
                    "Unable to read ACCEPT_CH origin."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x01"      // length
                    "\x05",     // valid origin length but no origin string
                    "Unable to read ACCEPT_CH origin."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x04"      // length
                    "\x05"      // valid origin length
                    "foo",      // payload ends before origin ends
                    "Unable to read ACCEPT_CH origin."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x04"      // length
                    "\x03"      // valid origin length
                    "foo",      // payload ends at end of origin: no value
                    "Unable to read ACCEPT_CH value."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x05"      // length
                    "\x03"      // valid origin length
                    "foo"       // payload ends at end of origin: no value
                    "\x40",     // first byte of two-byte varint value length
                    "Unable to read ACCEPT_CH value."},
                   {"\x40\x89"  // type (ACCEPT_CH)
                    "\x08"      // length
                    "\x03"      // valid origin length
                    "foo"       // origin
                    "\x05"      // valid value length
                    "bar",      // payload ends before value ends
                    "Unable to read ACCEPT_CH value."}};

  for (const auto& test_data : kTestData) {
    {
      HttpDecoder decoder(&visitor_);
      EXPECT_CALL(visitor_, OnAcceptChFrameStart(_)).Times(AnyNumber());
      EXPECT_CALL(visitor_, OnError(&decoder));

      absl::string_view input(test_data.input);
      decoder.ProcessInput(input.data(), input.size());
      EXPECT_THAT(decoder.error(), IsError(QUIC_HTTP_FRAME_ERROR));
      EXPECT_EQ(test_data.error_message, decoder.error_detail());
    }
    {
      HttpDecoder decoder(&visitor_);
      EXPECT_CALL(visitor_, OnAcceptChFrameStart(_)).Times(AnyNumber());
      EXPECT_CALL(visitor_, OnError(&decoder));

      absl::string_view input(test_data.input);
      for (auto c : input) {
        decoder.ProcessInput(&c, 1);
      }
      EXPECT_THAT(decoder.error(), IsError(QUIC_HTTP_FRAME_ERROR));
      EXPECT_EQ(test_data.error_message, decoder.error_detail());
    }
  }
}

TEST_F(HttpDecoderTest, EmptySettingsFrame) {
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"   // type (SETTINGS)
                             "00",  // frame length
                             &input));

  EXPECT_CALL(visitor_, OnSettingsFrameStart(2));

  SettingsFrame empty_frame;
  EXPECT_CALL(visitor_, OnSettingsFrame(empty_frame));

  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyGoAwayFrame) {
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("07"   // type (GOAWAY)
                             "00",  // frame length
                             &input));

  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_ERROR));
  EXPECT_EQ("Unable to read GOAWAY ID.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, EmptyMaxPushIdFrame) {
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("0d"   // type (MAX_PUSH_ID)
                             "00",  // frame length
                             &input));

  EXPECT_CALL(visitor_, OnError(&decoder_));
  EXPECT_EQ(input.size(), ProcessInput(input));
  EXPECT_THAT(decoder_.error(), IsError(QUIC_HTTP_FRAME_ERROR));
  EXPECT_EQ("Unable to read MAX_PUSH_ID push_id.", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, LargeStreamIdInGoAway) {
  GoAwayFrame frame;
  frame.id = 1ull << 60;
  std::string goaway = HttpEncoder::SerializeGoAwayFrame(frame);
  EXPECT_CALL(visitor_, OnGoAwayFrame(frame));
  EXPECT_GT(goaway.length(), 0u);
  EXPECT_EQ(goaway.length(),
            decoder_.ProcessInput(goaway.data(), goaway.length()));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

// Old PRIORITY_UPDATE frame is parsed as unknown frame.
TEST_F(HttpDecoderTest, ObsoletePriorityUpdateFrame) {
  const QuicByteCount header_length = 2;
  const QuicByteCount payload_length = 3;
  InSequence s;
  std::string input;
  ASSERT_TRUE(
      absl::HexStringToBytes("0f"       // type (obsolete PRIORITY_UPDATE)
                             "03"       // length
                             "666f6f",  // payload "foo"
                             &input));

  // Process frame as a whole.
  EXPECT_CALL(visitor_,
              OnUnknownFrameStart(0x0f, header_length, payload_length));
  EXPECT_CALL(visitor_, OnUnknownFramePayload(Eq("foo")));
  EXPECT_CALL(visitor_, OnUnknownFrameEnd()).WillOnce(Return(false));

  EXPECT_EQ(header_length + payload_length,
            ProcessInputWithGarbageAppended(input));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process frame byte by byte.
  EXPECT_CALL(visitor_,
              OnUnknownFrameStart(0x0f, header_length, payload_length));
  EXPECT_CALL(visitor_, OnUnknownFramePayload(Eq("f")));
  EXPECT_CALL(visitor_, OnUnknownFramePayload(Eq("o")));
  EXPECT_CALL(visitor_, OnUnknownFramePayload(Eq("o")));
  EXPECT_CALL(visitor_, OnUnknownFrameEnd());

  ProcessInputCharByChar(input);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, PriorityUpdateFrame) {
  InSequence s;
  std::string input1;
  ASSERT_TRUE(
      absl::HexStringToBytes("800f0700"  // type (PRIORITY_UPDATE)
                             "01"        // length
                             "03",       // prioritized element id
                             &input1));

  PriorityUpdateFrame priority_update1;
  priority_update1.prioritized_element_id = 0x03;

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5)).WillOnce(Return(false));
  absl::string_view remaining_input(input1);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(5u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update1))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5));
  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update1));
  EXPECT_EQ(input1.size(), ProcessInput(input1));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5));
  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update1));
  ProcessInputCharByChar(input1);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  std::string input2;
  ASSERT_TRUE(
      absl::HexStringToBytes("800f0700"  // type (PRIORITY_UPDATE)
                             "04"        // length
                             "05"        // prioritized element id
                             "666f6f",   // priority field value: "foo"
                             &input2));

  PriorityUpdateFrame priority_update2;
  priority_update2.prioritized_element_id = 0x05;
  priority_update2.priority_field_value = "foo";

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5)).WillOnce(Return(false));
  remaining_input = input2;
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(5u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update2))
      .WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5));
  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update2));
  EXPECT_EQ(input2.size(), ProcessInput(input2));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(5));
  EXPECT_CALL(visitor_, OnPriorityUpdateFrame(priority_update2));
  ProcessInputCharByChar(input2);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, CorruptPriorityUpdateFrame) {
  std::string payload;
  ASSERT_TRUE(absl::HexStringToBytes("4005",  // prioritized element id
                                     &payload));
  struct {
    size_t payload_length;
    const char* const error_message;
  } kTestData[] = {
      {0, "Unable to read prioritized element id."},
      {1, "Unable to read prioritized element id."},
  };

  for (const auto& test_data : kTestData) {
    std::string input;
    ASSERT_TRUE(absl::HexStringToBytes("800f0700",  // type PRIORITY_UPDATE
                                       &input));
    input.push_back(test_data.payload_length);
    size_t header_length = input.size();
    input.append(payload.data(), test_data.payload_length);

    HttpDecoder decoder(&visitor_);
    EXPECT_CALL(visitor_, OnPriorityUpdateFrameStart(header_length));
    EXPECT_CALL(visitor_, OnError(&decoder));

    QuicByteCount processed_bytes =
        decoder.ProcessInput(input.data(), input.size());
    EXPECT_EQ(input.size(), processed_bytes);
    EXPECT_THAT(decoder.error(), IsError(QUIC_HTTP_FRAME_ERROR));
    EXPECT_EQ(test_data.error_message, decoder.error_detail());
  }
}

TEST_F(HttpDecoderTest, AcceptChFrame) {
  InSequence s;
  std::string input1;
  ASSERT_TRUE(
      absl::HexStringToBytes("4089"  // type (ACCEPT_CH)
                             "00",   // length
                             &input1));

  AcceptChFrame accept_ch1;

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3)).WillOnce(Return(false));
  absl::string_view remaining_input(input1);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(3u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch1)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3));
  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch1));
  EXPECT_EQ(input1.size(), ProcessInput(input1));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3));
  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch1));
  ProcessInputCharByChar(input1);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  std::string input2;
  ASSERT_TRUE(
      absl::HexStringToBytes("4089"     // type (ACCEPT_CH)
                             "08"       // length
                             "03"       // length of origin
                             "666f6f"   // origin "foo"
                             "03"       // length of value
                             "626172",  // value "bar"
                             &input2));

  AcceptChFrame accept_ch2;
  accept_ch2.entries.push_back({"foo", "bar"});

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3)).WillOnce(Return(false));
  remaining_input = input2;
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(3u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch2)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3));
  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch2));
  EXPECT_EQ(input2.size(), ProcessInput(input2));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnAcceptChFrameStart(3));
  EXPECT_CALL(visitor_, OnAcceptChFrame(accept_ch2));
  ProcessInputCharByChar(input2);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, OriginFrame) {
  if (!GetQuicReloadableFlag(enable_h3_origin_frame)) {
    return;
  }
  InSequence s;
  std::string input1;
  ASSERT_TRUE(
      absl::HexStringToBytes("0C"   // type (ORIGIN)
                             "00",  // length
                             &input1));

  OriginFrame origin1;

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2)).WillOnce(Return(false));
  absl::string_view remaining_input(input1);
  QuicByteCount processed_bytes =
      ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnOriginFrame(origin1)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2));
  EXPECT_CALL(visitor_, OnOriginFrame(origin1));
  EXPECT_EQ(input1.size(), ProcessInput(input1));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2));
  EXPECT_CALL(visitor_, OnOriginFrame(origin1));
  ProcessInputCharByChar(input1);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  std::string input2;
  ASSERT_TRUE(
      absl::HexStringToBytes("0C"       // type (ORIGIN)
                             "0A"       // length
                             "0003"     // length of origin
                             "666f6f"   // origin "foo"
                             "0003"     // length of origin
                             "626172",  // origin "bar"
                             &input2));
  ASSERT_EQ(12, input2.length());

  OriginFrame origin2;
  origin2.origins = {"foo", "bar"};

  // Visitor pauses processing.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2)).WillOnce(Return(false));
  remaining_input = input2;
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(2u, processed_bytes);
  remaining_input = remaining_input.substr(processed_bytes);

  EXPECT_CALL(visitor_, OnOriginFrame(origin2)).WillOnce(Return(false));
  processed_bytes = ProcessInputWithGarbageAppended(remaining_input);
  EXPECT_EQ(remaining_input.size(), processed_bytes);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the full frame.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2));
  EXPECT_CALL(visitor_, OnOriginFrame(origin2));
  EXPECT_EQ(input2.size(), ProcessInput(input2));
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());

  // Process the frame incrementally.
  EXPECT_CALL(visitor_, OnOriginFrameStart(2));
  EXPECT_CALL(visitor_, OnOriginFrame(origin2));
  ProcessInputCharByChar(input2);
  EXPECT_THAT(decoder_.error(), IsQuicNoError());
  EXPECT_EQ("", decoder_.error_detail());
}

TEST_F(HttpDecoderTest, OriginFrameDisabled) {
  if (GetQuicReloadableFlag(enable_h3_origin_frame)) {
    return;
  }
  InSequence s;

  std::string input1;
  ASSERT_TRUE(
      absl::HexStringToBytes("0C"   // type (ORIGIN)
                             "00",  // length
                             &input1));
  EXPECT_CALL(visitor_, OnUnknownFrameStart(0x0C, 2, 0));
  EXPECT_CALL(visitor_, OnUnknownFrameEnd());
  EXPECT_EQ(ProcessInput(input1), input1.size());

  std::string input2;
  ASSERT_TRUE(
      absl::HexStringToBytes("0C"       // type (ORIGIN)
                             "0A"       // length
                             "0003"     // length of origin
                             "666f6f"   // origin "foo"
                             "0003"     // length of origin
                             "626172",  // origin "bar"
                             &input2));
  EXPECT_CALL(visitor_, OnUnknownFrameStart(0x0C, 2, input2.size() - 2));
  EXPECT_CALL(visitor_, OnUnknownFramePayload(input2.substr(2)));
  EXPECT_CALL(visitor_, OnUnknownFrameEnd());
  EXPECT_EQ(ProcessInput(input2), input2.size());
}

TEST_F(HttpDecoderTest, WebTransportStreamDisabled) {
  InSequence s;

  // Unknown frame of type 0x41 and length 0x104.
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes("40414104", &input));
  EXPECT_CALL(visitor_, OnUnknownFrameStart(0x41, input.size(), 0x104));
  EXPECT_EQ(ProcessInput(input), input.size());
}

TEST(HttpDecoderTestNoFixture, WebTransportStream) {
  testing::StrictMock<MockHttpDecoderVisitor> visitor;
  HttpDecoder decoder(&visitor);
  decoder.EnableWebTransportStreamParsing();

  // WebTransport stream for session ID 0x104, with four bytes of extra data.
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes("40414104ffffffff", &input));
  EXPECT_CALL(visitor, OnWebTransportStreamFrameType(4, 0x104));
  QuicByteCount bytes = decoder.ProcessInput(input.data(), input.size());
  EXPECT_EQ(bytes, 4u);
}

TEST(HttpDecoderTestNoFixture, WebTransportStreamError) {
  testing::StrictMock<MockHttpDecoderVisitor> visitor;
  HttpDecoder decoder(&visitor);
  decoder.EnableWebTransportStreamParsing();

  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes("404100", &input));
  EXPECT_CALL(visitor, OnWebTransportStreamFrameType(_, _));
  decoder.ProcessInput(input.data(), input.size());

  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(visitor, OnError(_));
        decoder.ProcessInput(input.data(), input.size());
      },
      "HttpDecoder called after an indefinite-length frame");
}

TEST_F(HttpDecoderTest, DecodeSettings) {
  std::string input;
  ASSERT_TRUE(absl::HexStringToBytes(
      "04"    // type (SETTINGS)
      "07"    // length
      "01"    // identifier (SETTINGS_QPACK_MAX_TABLE_CAPACITY)
      "02"    // content
      "06"    // identifier (SETTINGS_MAX_HEADER_LIST_SIZE)
      "05"    // content
      "4100"  // identifier, encoded on 2 bytes (0x40), value is 256 (0x100)
      "04",   // content
      &input));

  SettingsFrame frame;
  frame.values[1] = 2;
  frame.values[6] = 5;
  frame.values[256] = 4;

  SettingsFrame out;
  EXPECT_TRUE(HttpDecoder::DecodeSettings(input.data(), input.size(), &out));
  EXPECT_EQ(frame, out);

  // non-settings frame.
  ASSERT_TRUE(
      absl::HexStringToBytes("0D"   // type (MAX_PUSH_ID)
                             "01"   // length
                             "01",  // Push Id
                             &input));

  EXPECT_FALSE(HttpDecoder::DecodeSettings(input.data(), input.size(), &out));

  // Corrupt SETTINGS.
  ASSERT_TRUE(absl::HexStringToBytes(
      "04"   // type (SETTINGS)
      "01"   // length
      "42",  // First byte of setting identifier, indicating a 2-byte varint62.
      &input));

  EXPECT_FALSE(HttpDecoder::DecodeSettings(input.data(), input.size(), &out));
}

}  // namespace
}  // namespace test
}  // namespace quic
