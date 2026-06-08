// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/qpack/qpack_decoder_stream_sender.h"

#include <string>

#include "absl/strings/escaping.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"

using ::testing::Eq;
using ::testing::StrictMock;

namespace quic {
namespace test {
namespace {

class QpackDecoderStreamSenderTest : public QuicTest {
 protected:
  QpackDecoderStreamSenderTest() {
    stream_.set_qpack_stream_sender_delegate(&delegate_);
  }
  ~QpackDecoderStreamSenderTest() override = default;

  StrictMock<MockQpackStreamSenderDelegate> delegate_;
  QpackDecoderStreamSender stream_;
};

TEST_F(QpackDecoderStreamSenderTest, InsertCountIncrement) {
  std::string stream_data;
  ASSERT_TRUE(absl::HexStringToBytes("00", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendInsertCountIncrement(0);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("0a", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendInsertCountIncrement(10);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("3f00", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendInsertCountIncrement(63);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("3f8901", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendInsertCountIncrement(200);
  stream_.Flush();
}

TEST_F(QpackDecoderStreamSenderTest, HeaderAcknowledgement) {
  std::string stream_data;
  ASSERT_TRUE(absl::HexStringToBytes("80", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendHeaderAcknowledgement(0);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("a5", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendHeaderAcknowledgement(37);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("ff00", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendHeaderAcknowledgement(127);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("fff802", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendHeaderAcknowledgement(503);
  stream_.Flush();
}

TEST_F(QpackDecoderStreamSenderTest, StreamCancellation) {
  std::string stream_data;
  ASSERT_TRUE(absl::HexStringToBytes("40", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendStreamCancellation(0);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("53", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendStreamCancellation(19);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("7f00", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendStreamCancellation(63);
  stream_.Flush();

  ASSERT_TRUE(absl::HexStringToBytes("7f2f", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.SendStreamCancellation(110);
  stream_.Flush();
}

TEST_F(QpackDecoderStreamSenderTest, Coalesce) {
  std::string stream_data;
  stream_.SendInsertCountIncrement(10);
  stream_.SendHeaderAcknowledgement(37);
  stream_.SendStreamCancellation(0);

  ASSERT_TRUE(absl::HexStringToBytes("0aa540", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.Flush();

  stream_.SendInsertCountIncrement(63);
  stream_.SendStreamCancellation(110);

  ASSERT_TRUE(absl::HexStringToBytes("3f007f2f", &stream_data));
  EXPECT_CALL(delegate_, WriteStreamData(Eq(stream_data)));
  stream_.Flush();
}

}  // namespace
}  // namespace test
}  // namespace quic
