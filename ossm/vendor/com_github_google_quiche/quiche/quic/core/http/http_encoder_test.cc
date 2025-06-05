// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/http_encoder.h"

#include <string>

#include "absl/base/macros.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quic {
namespace test {

TEST(HttpEncoderTest, SerializeDataFrameHeader) {
  quiche::QuicheBuffer buffer = HttpEncoder::SerializeDataFrameHeader(
      /* payload_length = */ 5, quiche::SimpleBufferAllocator::Get());
  char output[] = {0x00,   // type (DATA)
                   0x05};  // length
  EXPECT_EQ(ABSL_ARRAYSIZE(output), buffer.size());
  quiche::test::CompareCharArraysWithHexError(
      "DATA", buffer.data(), buffer.size(), output, ABSL_ARRAYSIZE(output));
}

TEST(HttpEncoderTest, SerializeHeadersFrameHeader) {
  std::string header =
      HttpEncoder::SerializeHeadersFrameHeader(/* payload_length = */ 7);
  char output[] = {0x01,   // type (HEADERS)
                   0x07};  // length
  quiche::test::CompareCharArraysWithHexError("HEADERS", header.data(),
                                              header.length(), output,
                                              ABSL_ARRAYSIZE(output));
}

TEST(HttpEncoderTest, SerializeSettingsFrame) {
  SettingsFrame settings;
  settings.values[1] = 2;
  settings.values[6] = 5;
  settings.values[256] = 4;
  char output[] = {0x04,  // type (SETTINGS)
                   0x07,  // length
                   0x01,  // identifier (SETTINGS_QPACK_MAX_TABLE_CAPACITY)
                   0x02,  // content
                   0x06,  // identifier (SETTINGS_MAX_HEADER_LIST_SIZE)
                   0x05,  // content
                   0x41, 0x00,  // identifier 0x100, varint encoded
                   0x04};       // content
  std::string frame = HttpEncoder::SerializeSettingsFrame(settings);
  quiche::test::CompareCharArraysWithHexError(
      "SETTINGS", frame.data(), frame.length(), output, ABSL_ARRAYSIZE(output));
}

TEST(HttpEncoderTest, SerializeGoAwayFrame) {
  GoAwayFrame goaway;
  goaway.id = 0x1;
  char output[] = {0x07,   // type (GOAWAY)
                   0x1,    // length
                   0x01};  // ID
  std::string frame = HttpEncoder::SerializeGoAwayFrame(goaway);
  quiche::test::CompareCharArraysWithHexError(
      "GOAWAY", frame.data(), frame.length(), output, ABSL_ARRAYSIZE(output));
}

TEST(HttpEncoderTest, SerializePriorityUpdateFrame) {
  PriorityUpdateFrame priority_update1;
  priority_update1.prioritized_element_id = 0x03;
  uint8_t output1[] = {0x80, 0x0f, 0x07, 0x00,  // type (PRIORITY_UPDATE)
                       0x01,                    // length
                       0x03};                   // prioritized element id

  std::string frame1 =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update1);
  quiche::test::CompareCharArraysWithHexError(
      "PRIORITY_UPDATE", frame1.data(), frame1.length(),
      reinterpret_cast<char*>(output1), ABSL_ARRAYSIZE(output1));

  PriorityUpdateFrame priority_update2;
  priority_update2.prioritized_element_id = 0x05;
  priority_update2.priority_field_value = "foo";

  uint8_t output2[] = {0x80, 0x0f, 0x07, 0x00,  // type (PRIORITY_UPDATE)
                       0x04,                    // length
                       0x05,                    // prioritized element id
                       0x66, 0x6f, 0x6f};       // priority field value: "foo"

  std::string frame2 =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update2);
  quiche::test::CompareCharArraysWithHexError(
      "PRIORITY_UPDATE", frame2.data(), frame2.length(),
      reinterpret_cast<char*>(output2), ABSL_ARRAYSIZE(output2));
}

TEST(HttpEncoderTest, SerializeEmptyOriginFrame) {
  OriginFrame frame;
  uint8_t expected[] = {0x0C,   // type (ACCEPT_CH)
                        0x00};  // length

  std::string output = HttpEncoder::SerializeOriginFrame(frame);
  quiche::test::CompareCharArraysWithHexError(
      "ORIGIN", output.data(), output.length(),
      reinterpret_cast<char*>(expected), ABSL_ARRAYSIZE(expected));
}

TEST(HttpEncoderTest, SerializeOriginFrame) {
  OriginFrame frame;
  frame.origins = {"foo", "bar"};
  uint8_t expected[] = {0x0C,                // type (ORIGIN)
                        0x0A,                // length
                        0x00, 0x003,         // length of origin
                        0x66, 0x6f,  0x6f,   // origin "foo"
                        0x00, 0x003,         // length of origin
                        0x62, 0x61,  0x72};  // origin "bar"

  std::string output = HttpEncoder::SerializeOriginFrame(frame);
  quiche::test::CompareCharArraysWithHexError(
      "ORIGIN", output.data(), output.length(),
      reinterpret_cast<char*>(expected), ABSL_ARRAYSIZE(expected));
}

TEST(HttpEncoderTest, SerializeAcceptChFrame) {
  AcceptChFrame accept_ch;
  uint8_t output1[] = {0x40, 0x89,  // type (ACCEPT_CH)
                       0x00};       // length

  std::string frame1 = HttpEncoder::SerializeAcceptChFrame(accept_ch);
  quiche::test::CompareCharArraysWithHexError(
      "ACCEPT_CH", frame1.data(), frame1.length(),
      reinterpret_cast<char*>(output1), ABSL_ARRAYSIZE(output1));

  accept_ch.entries.push_back({"foo", "bar"});
  uint8_t output2[] = {0x40, 0x89,               // type (ACCEPT_CH)
                       0x08,                     // payload length
                       0x03, 0x66, 0x6f, 0x6f,   // length of "foo"; "foo"
                       0x03, 0x62, 0x61, 0x72};  // length of "bar"; "bar"

  std::string frame2 = HttpEncoder::SerializeAcceptChFrame(accept_ch);
  quiche::test::CompareCharArraysWithHexError(
      "ACCEPT_CH", frame2.data(), frame2.length(),
      reinterpret_cast<char*>(output2), ABSL_ARRAYSIZE(output2));
}

TEST(HttpEncoderTest, SerializeWebTransportStreamFrameHeader) {
  WebTransportSessionId session_id = 0x17;
  char output[] = {0x40, 0x41,  // type (WEBTRANSPORT_STREAM)
                   0x17};       // session ID

  std::string frame =
      HttpEncoder::SerializeWebTransportStreamFrameHeader(session_id);
  quiche::test::CompareCharArraysWithHexError("WEBTRANSPORT_STREAM",
                                              frame.data(), frame.length(),
                                              output, sizeof(output));
}

TEST(HttpEncoderTest, SerializeMetadataFrameHeader) {
  std::string frame = HttpEncoder::SerializeMetadataFrameHeader(
      /* payload_length = */ 7);
  char output[] = {0x40, 0x4d,  // type (METADATA, 0x4d, varint encoded)
                   0x07};       // length
  quiche::test::CompareCharArraysWithHexError(
      "METADATA", frame.data(), frame.length(), output, ABSL_ARRAYSIZE(output));
}

}  // namespace test
}  // namespace quic
