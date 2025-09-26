// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/qpack/qpack_decoder_stream_receiver.h"

#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_test.h"

using testing::Eq;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockDelegate : public QpackDecoderStreamReceiver::Delegate {
 public:
  ~MockDelegate() override = default;

  MOCK_METHOD(void, OnInsertCountIncrement, (uint64_t increment), (override));
  MOCK_METHOD(void, OnHeaderAcknowledgement, (QuicStreamId stream_id),
              (override));
  MOCK_METHOD(void, OnStreamCancellation, (QuicStreamId stream_id), (override));
  MOCK_METHOD(void, OnErrorDetected,
              (QuicErrorCode error_code, absl::string_view error_message),
              (override));
};

class QpackDecoderStreamReceiverTest : public QuicTest {
 protected:
  QpackDecoderStreamReceiverTest() : stream_(&delegate_) {}
  ~QpackDecoderStreamReceiverTest() override = default;

  QpackDecoderStreamReceiver stream_;
  StrictMock<MockDelegate> delegate_;
};

TEST_F(QpackDecoderStreamReceiverTest, InsertCountIncrement) {
  std::string encoded_data;
  EXPECT_CALL(delegate_, OnInsertCountIncrement(0));
  ASSERT_TRUE(absl::HexStringToBytes("00", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnInsertCountIncrement(10));
  ASSERT_TRUE(absl::HexStringToBytes("0a", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnInsertCountIncrement(63));
  ASSERT_TRUE(absl::HexStringToBytes("3f00", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnInsertCountIncrement(200));
  ASSERT_TRUE(absl::HexStringToBytes("3f8901", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_,
              OnErrorDetected(QUIC_QPACK_DECODER_STREAM_INTEGER_TOO_LARGE,
                              Eq("Encoded integer too large.")));
  ASSERT_TRUE(absl::HexStringToBytes("3fffffffffffffffffffff", &encoded_data));
  stream_.Decode(encoded_data);
}

TEST_F(QpackDecoderStreamReceiverTest, HeaderAcknowledgement) {
  std::string encoded_data;
  EXPECT_CALL(delegate_, OnHeaderAcknowledgement(0));
  ASSERT_TRUE(absl::HexStringToBytes("80", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnHeaderAcknowledgement(37));
  ASSERT_TRUE(absl::HexStringToBytes("a5", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnHeaderAcknowledgement(127));
  ASSERT_TRUE(absl::HexStringToBytes("ff00", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnHeaderAcknowledgement(503));
  ASSERT_TRUE(absl::HexStringToBytes("fff802", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_,
              OnErrorDetected(QUIC_QPACK_DECODER_STREAM_INTEGER_TOO_LARGE,
                              Eq("Encoded integer too large.")));
  ASSERT_TRUE(absl::HexStringToBytes("ffffffffffffffffffffff", &encoded_data));
  stream_.Decode(encoded_data);
}

TEST_F(QpackDecoderStreamReceiverTest, StreamCancellation) {
  std::string encoded_data;
  EXPECT_CALL(delegate_, OnStreamCancellation(0));
  ASSERT_TRUE(absl::HexStringToBytes("40", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnStreamCancellation(19));
  ASSERT_TRUE(absl::HexStringToBytes("53", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnStreamCancellation(63));
  ASSERT_TRUE(absl::HexStringToBytes("7f00", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_, OnStreamCancellation(110));
  ASSERT_TRUE(absl::HexStringToBytes("7f2f", &encoded_data));
  stream_.Decode(encoded_data);

  EXPECT_CALL(delegate_,
              OnErrorDetected(QUIC_QPACK_DECODER_STREAM_INTEGER_TOO_LARGE,
                              Eq("Encoded integer too large.")));
  ASSERT_TRUE(absl::HexStringToBytes("7fffffffffffffffffffff", &encoded_data));
  stream_.Decode(encoded_data);
}

}  // namespace
}  // namespace test
}  // namespace quic
