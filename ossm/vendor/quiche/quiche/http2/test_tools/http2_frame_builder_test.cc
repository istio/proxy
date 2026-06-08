// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/http2/test_tools/http2_frame_builder.h"

#include <string>

#include "absl/strings/escaping.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace http2 {
namespace test {
namespace {

const char kHighBitSetMsg[] = "High-bit of uint32_t should be clear";

TEST(Http2FrameBuilderTest, Constructors) {
  {
    Http2FrameBuilder fb;
    EXPECT_EQ(0u, fb.size());
  }
  {
    Http2FrameBuilder fb(Http2FrameType::DATA, 0, 123);
    EXPECT_EQ(9u, fb.size());

    std::string expected_data;
    ASSERT_TRUE(
        absl::HexStringToBytes("000000"     // Payload length: 0 (unset)
                               "00"         // Frame type: DATA
                               "00"         // Flags: none
                               "0000007b",  // Stream ID: 123
                               &expected_data));
    EXPECT_EQ(expected_data, fb.buffer());
  }
  {
    Http2FrameHeader header;
    header.payload_length = (1 << 24) - 1;
    header.type = Http2FrameType::HEADERS;
    header.flags = Http2FrameFlag::END_HEADERS;
    header.stream_id = StreamIdMask();
    Http2FrameBuilder fb(header);
    EXPECT_EQ(9u, fb.size());

    std::string expected_data;
    ASSERT_TRUE(absl::HexStringToBytes(
        "ffffff"     // Payload length: 2^24 - 1 (max uint24)
        "01"         // Frame type: HEADER
        "04"         // Flags: END_HEADERS
        "7fffffff",  // Stream ID: stream id mask
        &expected_data));
    EXPECT_EQ(expected_data, fb.buffer());
  }
}

TEST(Http2FrameBuilderTest, SetPayloadLength) {
  Http2FrameBuilder fb(Http2FrameType::DATA, PADDED, 20000);
  EXPECT_EQ(9u, fb.size());

  fb.AppendUInt8(50);  // Trailing payload length
  EXPECT_EQ(10u, fb.size());

  fb.Append("ten bytes.");
  EXPECT_EQ(20u, fb.size());

  fb.AppendZeroes(50);
  EXPECT_EQ(70u, fb.size());

  fb.SetPayloadLength();
  EXPECT_EQ(70u, fb.size());

  std::string expected_data;
  ASSERT_TRUE(
      absl::HexStringToBytes("00003d"                 // Payload length: 61
                             "00"                     // Frame type: DATA
                             "08"                     // Flags: PADDED
                             "00004e20"               // Stream ID: 20000
                             "32"                     // Padding Length: 50
                             "74656e2062797465732e"   // "ten bytes."
                             "00000000000000000000"   // Padding bytes
                             "00000000000000000000"   // Padding bytes
                             "00000000000000000000"   // Padding bytes
                             "00000000000000000000"   // Padding bytes
                             "00000000000000000000",  // Padding bytes
                             &expected_data));
  EXPECT_EQ(expected_data, fb.buffer());
}

TEST(Http2FrameBuilderTest, Settings) {
  Http2FrameBuilder fb(Http2FrameType::SETTINGS, 0, 0);
  Http2SettingFields sf;

  sf.parameter = Http2SettingsParameter::HEADER_TABLE_SIZE;
  sf.value = 1 << 12;
  fb.Append(sf);

  sf.parameter = Http2SettingsParameter::ENABLE_PUSH;
  sf.value = 0;
  fb.Append(sf);

  sf.parameter = Http2SettingsParameter::MAX_CONCURRENT_STREAMS;
  sf.value = ~0;
  fb.Append(sf);

  sf.parameter = Http2SettingsParameter::INITIAL_WINDOW_SIZE;
  sf.value = 1 << 16;
  fb.Append(sf);

  sf.parameter = Http2SettingsParameter::MAX_FRAME_SIZE;
  sf.value = 1 << 14;
  fb.Append(sf);

  sf.parameter = Http2SettingsParameter::MAX_HEADER_LIST_SIZE;
  sf.value = 1 << 10;
  fb.Append(sf);

  size_t payload_size = 6 * Http2SettingFields::EncodedSize();
  EXPECT_EQ(Http2FrameHeader::EncodedSize() + payload_size, fb.size());

  fb.SetPayloadLength(payload_size);

  std::string expected_data;
  ASSERT_TRUE(
      absl::HexStringToBytes("000024"     // Payload length: 36
                             "04"         // Frame type: SETTINGS
                             "00"         // Flags: none
                             "00000000"   // Stream ID: 0
                             "0001"       // HEADER_TABLE_SIZE
                             "00001000"   // 4096
                             "0002"       // ENABLE_PUSH
                             "00000000"   // 0
                             "0003"       // MAX_CONCURRENT_STREAMS
                             "ffffffff"   // 0xffffffff (max uint32)
                             "0004"       // INITIAL_WINDOW_SIZE
                             "00010000"   // 4096
                             "0005"       // MAX_FRAME_SIZE
                             "00004000"   // 4096
                             "0006"       // MAX_HEADER_LIST_SIZE
                             "00000400",  // 1024
                             &expected_data));
  EXPECT_EQ(expected_data, fb.buffer());
}

TEST(Http2FrameBuilderTest, EnhanceYourCalm) {
  std::string expected_data;
  ASSERT_TRUE(absl::HexStringToBytes("0000000b", &expected_data));
  {
    Http2FrameBuilder fb;
    fb.Append(Http2ErrorCode::ENHANCE_YOUR_CALM);
    EXPECT_EQ(expected_data, fb.buffer());
  }
  {
    Http2FrameBuilder fb;
    Http2RstStreamFields rsp;
    rsp.error_code = Http2ErrorCode::ENHANCE_YOUR_CALM;
    fb.Append(rsp);
    EXPECT_EQ(expected_data, fb.buffer());
  }
}

TEST(Http2FrameBuilderTest, PushPromise) {
  std::string expected_data;
  ASSERT_TRUE(absl::HexStringToBytes("7fffffff", &expected_data));
  {
    Http2FrameBuilder fb;
    fb.Append(Http2PushPromiseFields{0x7fffffff});
    EXPECT_EQ(expected_data, fb.buffer());
  }
  {
    Http2FrameBuilder fb;
    // Will generate an error if the high-bit of the stream id is set.
    EXPECT_NONFATAL_FAILURE(fb.Append(Http2PushPromiseFields{0xffffffff}),
                            kHighBitSetMsg);
    EXPECT_EQ(expected_data, fb.buffer());
  }
}

TEST(Http2FrameBuilderTest, Ping) {
  Http2FrameBuilder fb;
  Http2PingFields ping{"8 bytes"};
  fb.Append(ping);

  const absl::string_view kData{"8 bytes\0", 8};
  EXPECT_EQ(kData.size(), Http2PingFields::EncodedSize());
  EXPECT_EQ(kData, fb.buffer());
}

TEST(Http2FrameBuilderTest, GoAway) {
  std::string expected_data;
  ASSERT_TRUE(
      absl::HexStringToBytes("12345678"   // Last Stream Id
                             "00000001",  // Error code
                             &expected_data));
  EXPECT_EQ(expected_data.size(), Http2GoAwayFields::EncodedSize());
  {
    Http2FrameBuilder fb;
    Http2GoAwayFields ga(0x12345678, Http2ErrorCode::PROTOCOL_ERROR);
    fb.Append(ga);
    EXPECT_EQ(expected_data, fb.buffer());
  }
  {
    Http2FrameBuilder fb;
    // Will generate a test failure if the high-bit of the stream id is set.
    Http2GoAwayFields ga(0x92345678, Http2ErrorCode::PROTOCOL_ERROR);
    EXPECT_NONFATAL_FAILURE(fb.Append(ga), kHighBitSetMsg);
    EXPECT_EQ(expected_data, fb.buffer());
  }
}

TEST(Http2FrameBuilderTest, WindowUpdate) {
  Http2FrameBuilder fb;
  fb.Append(Http2WindowUpdateFields{123456});

  // Will generate a test failure if the high-bit of the increment is set.
  EXPECT_NONFATAL_FAILURE(fb.Append(Http2WindowUpdateFields{0x80000001}),
                          kHighBitSetMsg);

  // Will generate a test failure if the increment is zero.
  EXPECT_NONFATAL_FAILURE(fb.Append(Http2WindowUpdateFields{0}), "non-zero");

  std::string expected_data;
  ASSERT_TRUE(
      absl::HexStringToBytes("0001e240"   // Valid Window Size Increment
                             "00000001"   // High-bit cleared
                             "00000000",  // Invalid Window Size Increment
                             &expected_data));
  EXPECT_EQ(expected_data.size(), 3 * Http2WindowUpdateFields::EncodedSize());
  EXPECT_EQ(expected_data, fb.buffer());
}

TEST(Http2FrameBuilderTest, AltSvc) {
  Http2FrameBuilder fb;
  fb.Append(Http2AltSvcFields{99});
  fb.Append(Http2AltSvcFields{0});  // No optional origin
  std::string expected_data;
  ASSERT_TRUE(
      absl::HexStringToBytes("0063"   // Has origin.
                             "0000",  // Doesn't have origin.
                             &expected_data));
  EXPECT_EQ(expected_data.size(), 2 * Http2AltSvcFields::EncodedSize());
  EXPECT_EQ(expected_data, fb.buffer());
}

}  // namespace
}  // namespace test
}  // namespace http2
