// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/metadata_decoder.h"

#include <string>

#include "absl/strings/escaping.h"
#include "quiche/quic/core/qpack/qpack_encoder.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

class MetadataDecoderTest : public QuicTest {
 protected:
  std::string EncodeHeaders(quiche::HttpHeaderBlock& headers) {
    quic::NoopDecoderStreamErrorDelegate delegate;
    quic::QpackEncoder encoder(&delegate, quic::HuffmanEncoding::kDisabled,
                               quic::CookieCrumbling::kDisabled);
    return encoder.EncodeHeaderList(id_, headers,
                                    /*encoder_stream_sent_byte_count=*/nullptr);
  }

  size_t max_header_list_size = 1 << 20;  // 1 MB
  const QuicStreamId id_ = 1;
};

TEST_F(MetadataDecoderTest, Initialize) {
  const size_t frame_header_len = 4;
  const size_t payload_len = 123;
  MetadataDecoder decoder(id_, max_header_list_size, frame_header_len,
                          payload_len);
  EXPECT_EQ(frame_header_len + payload_len, decoder.frame_len());
  EXPECT_EQ("", decoder.error_message());
  EXPECT_TRUE(decoder.headers().empty());
}

TEST_F(MetadataDecoderTest, Decode) {
  quiche::HttpHeaderBlock headers;
  headers["key1"] = "val1";
  headers["key2"] = "val2";
  headers["key3"] = "val3";
  std::string data = EncodeHeaders(headers);

  const size_t frame_header_len = 4;
  MetadataDecoder decoder(id_, max_header_list_size, frame_header_len,
                          data.length());
  EXPECT_TRUE(decoder.Decode(data));
  EXPECT_TRUE(decoder.EndHeaderBlock());
  EXPECT_EQ(quic::test::AsHeaderList(headers), decoder.headers());
}

TEST_F(MetadataDecoderTest, DecodeInvalidHeaders) {
  std::string data = "aaaaaaaaaa";

  const size_t frame_header_len = 4;
  MetadataDecoder decoder(id_, max_header_list_size, frame_header_len,
                          data.length());
  EXPECT_FALSE(decoder.Decode(data));
  EXPECT_EQ("Error decoding metadata: Error decoding Required Insert Count.",
            decoder.error_message());
}

TEST_F(MetadataDecoderTest, TooLarge) {
  quiche::HttpHeaderBlock headers;
  for (int i = 0; i < 1024; ++i) {
    headers.AppendValueOrAddHeader(absl::StrCat(i), std::string(1024, 'a'));
  }
  std::string data = EncodeHeaders(headers);

  EXPECT_GT(data.length(), 1 << 20);
  const size_t frame_header_len = 4;
  MetadataDecoder decoder(id_, max_header_list_size, frame_header_len,
                          data.length());
  EXPECT_TRUE(decoder.Decode(data));
  EXPECT_FALSE(decoder.EndHeaderBlock());
  EXPECT_TRUE(decoder.error_message().empty());
}

}  // namespace
}  // namespace test
}  // namespace quic
