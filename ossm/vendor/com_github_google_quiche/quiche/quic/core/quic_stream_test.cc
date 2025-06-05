// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_stream.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/frames/quic_connection_close_frame.h"
#include "quiche/quic/core/frames/quic_reset_stream_at_frame.h"
#include "quiche/quic/core/frames/quic_rst_stream_frame.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_stream_sequencer.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/core/quic_write_blocked_list.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_flow_controller_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_stream_sequencer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_mem_slice_storage.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const QuicByteCount kDataLen = 9;
const uint8_t kPacket0ByteConnectionId = 0;
const uint8_t kPacket8ByteConnectionId = 8;

class TestStream : public QuicStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session, StreamType type)
      : QuicStream(id, session, /*is_static=*/false, type) {
    sequencer()->set_level_triggered(true);
  }

  TestStream(PendingStream* pending, QuicSession* session, bool is_static)
      : QuicStream(pending, session, is_static) {}

  MOCK_METHOD(void, OnDataAvailable, (), (override));

  MOCK_METHOD(void, OnCanWriteNewData, (), (override));

  MOCK_METHOD(void, OnWriteSideInDataRecvdState, (), (override));

  using QuicStream::CanWriteNewData;
  using QuicStream::CanWriteNewDataAfterData;
  using QuicStream::CloseWriteSide;
  using QuicStream::fin_buffered;
  using QuicStream::MaybeSendStopSending;
  using QuicStream::OnClose;
  using QuicStream::WriteMemSlices;
  using QuicStream::WriteOrBufferData;

  void ConsumeData(size_t num_bytes) {
    char buffer[1024];
    ASSERT_GT(ABSL_ARRAYSIZE(buffer), num_bytes);
    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = num_bytes;
    ASSERT_EQ(num_bytes, QuicStreamPeer::sequencer(this)->Readv(&iov, 1));
  }

  QuicStreamSequencer* sequencer() { return QuicStream::sequencer(); }

 private:
  std::string data_;
};

class QuicStreamTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicStreamTest()
      : zero_(QuicTime::Delta::Zero()),
        supported_versions_(AllSupportedVersions()) {}

  void Initialize(Perspective perspective = Perspective::IS_SERVER) {
    ParsedQuicVersionVector version_vector;
    version_vector.push_back(GetParam());
    connection_ = new StrictMock<MockQuicConnection>(
        &helper_, &alarm_factory_, perspective, version_vector);
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    session_ = std::make_unique<StrictMock<MockQuicSession>>(connection_);
    session_->Initialize();
    connection_->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<NullEncrypter>(connection_->perspective()));
    QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesUnidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesIncomingBidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesOutgoingBidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(session_->config(), 10);
    session_->config()->SetReliableStreamReset(true);
    session_->OnConfigNegotiated();

    stream_ = new StrictMock<TestStream>(kTestStreamId, session_.get(),
                                         BIDIRECTIONAL);
    EXPECT_NE(nullptr, stream_);
    EXPECT_CALL(*session_, ShouldKeepConnectionAlive())
        .WillRepeatedly(Return(true));
    // session_ now owns stream_.
    session_->ActivateStream(absl::WrapUnique(stream_));
    // Ignore resetting when session_ is terminated.
    EXPECT_CALL(*session_, MaybeSendStopSendingFrame(kTestStreamId, _))
        .Times(AnyNumber());
    EXPECT_CALL(*session_, MaybeSendRstStreamFrame(kTestStreamId, _, _))
        .Times(AnyNumber());
    write_blocked_list_ =
        QuicSessionPeer::GetWriteBlockedStreams(session_.get());
  }

  bool fin_sent() { return stream_->fin_sent(); }
  bool rst_sent() { return stream_->rst_sent(); }

  bool HasWriteBlockedStreams() {
    return write_blocked_list_->HasWriteBlockedSpecialStream() ||
           write_blocked_list_->HasWriteBlockedDataStreams();
  }

  QuicConsumedData CloseStreamOnWriteError(
      QuicStreamId id, QuicByteCount /*write_length*/,
      QuicStreamOffset /*offset*/, StreamSendingState /*state*/,
      TransmissionType /*type*/, std::optional<EncryptionLevel> /*level*/) {
    session_->ResetStream(id, QUIC_STREAM_CANCELLED);
    return QuicConsumedData(1, false);
  }

  bool ClearResetStreamFrame(const QuicFrame& frame) {
    EXPECT_EQ(RST_STREAM_FRAME, frame.type);
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

  bool ClearStopSendingFrame(const QuicFrame& frame) {
    EXPECT_EQ(STOP_SENDING_FRAME, frame.type);
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

  // Use application stream interface for sending data. This will trigger a call
  // to mock_stream->Writev(_, _) that will have to return QuicConsumedData.
  QuicConsumedData SendApplicationData(TestStream* stream,
                                       absl::string_view data, size_t iov_len,
                                       bool fin) {
    struct iovec iov = {const_cast<char*>(data.data()), iov_len};
    quiche::QuicheMemSliceStorage storage(
        &iov, 1,
        session_->connection()->helper()->GetStreamSendBufferAllocator(), 1024);
    return stream->WriteMemSlices(storage.ToSpan(), fin);
  }

  QuicConsumedData SendApplicationData(absl::string_view data, size_t iov_len,
                                       bool fin) {
    return SendApplicationData(stream_, data, iov_len, fin);
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  std::unique_ptr<MockQuicSession> session_;
  StrictMock<TestStream>* stream_;
  QuicWriteBlockedListInterface* write_blocked_list_;
  QuicTime::Delta zero_;
  ParsedQuicVersionVector supported_versions_;
  QuicStreamId kTestStreamId = GetNthClientInitiatedBidirectionalStreamId(
      GetParam().transport_version, 1);
  const QuicStreamId kTestPendingStreamId =
      GetNthClientInitiatedUnidirectionalStreamId(GetParam().transport_version,
                                                  1);
};

INSTANTIATE_TEST_SUITE_P(QuicStreamTests, QuicStreamTest,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

using PendingStreamTest = QuicStreamTest;

INSTANTIATE_TEST_SUITE_P(PendingStreamTests, PendingStreamTest,
                         ::testing::ValuesIn(CurrentSupportedHttp3Versions()),
                         ::testing::PrintToStringParamName());

TEST_P(PendingStreamTest, PendingStreamStaticness) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());
  TestStream stream(&pending, session_.get(), false);
  EXPECT_FALSE(stream.is_static());

  PendingStream pending2(kTestPendingStreamId + 4, session_.get());
  TestStream stream2(&pending2, session_.get(), true);
  EXPECT_TRUE(stream2.is_static());
}

TEST_P(PendingStreamTest, PendingStreamType) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());
  TestStream stream(&pending, session_.get(), false);
  EXPECT_EQ(stream.type(), READ_UNIDIRECTIONAL);
}

TEST_P(PendingStreamTest, PendingStreamTypeOnClient) {
  Initialize(Perspective::IS_CLIENT);

  QuicStreamId server_initiated_pending_stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(session_->transport_version(),
                                                  1);
  PendingStream pending(server_initiated_pending_stream_id, session_.get());
  TestStream stream(&pending, session_.get(), false);
  EXPECT_EQ(stream.type(), READ_UNIDIRECTIONAL);
}

TEST_P(PendingStreamTest, PendingStreamTooMuchData) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());
  // Receive a stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicStreamFrame frame(kTestPendingStreamId, false,
                        kInitialSessionFlowControlWindowForTest + 1, ".");

  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  pending.OnStreamFrame(frame);
}

TEST_P(PendingStreamTest, PendingStreamTooMuchDataInRstStream) {
  Initialize();

  PendingStream pending1(kTestPendingStreamId, session_.get());
  // Receive a rst stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicRstStreamFrame frame1(kInvalidControlFrameId, kTestPendingStreamId,
                            QUIC_STREAM_CANCELLED,
                            kInitialSessionFlowControlWindowForTest + 1);

  // Pending stream should not accept the frame, and the connection should be
  // closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  pending1.OnRstStreamFrame(frame1);

  QuicStreamId bidirection_stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      session_->transport_version(), Perspective::IS_CLIENT);
  PendingStream pending2(bidirection_stream_id, session_.get());
  // Receive a rst stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicRstStreamFrame frame2(kInvalidControlFrameId, bidirection_stream_id,
                            QUIC_STREAM_CANCELLED,
                            kInitialSessionFlowControlWindowForTest + 1);
  // Bidirectional Pending stream should not accept the frame, and the
  // connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  pending2.OnRstStreamFrame(frame2);
}

TEST_P(PendingStreamTest, PendingStreamRstStream) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());
  QuicStreamOffset final_byte_offset = 7;
  QuicRstStreamFrame frame(kInvalidControlFrameId, kTestPendingStreamId,
                           QUIC_STREAM_CANCELLED, final_byte_offset);

  // Pending stream should accept the frame and not close the connection.
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  pending.OnRstStreamFrame(frame);
}

TEST_P(PendingStreamTest, PendingStreamWindowUpdate) {
  Initialize();

  QuicStreamId bidirection_stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      session_->transport_version(), Perspective::IS_CLIENT);
  PendingStream pending(bidirection_stream_id, session_.get());
  QuicWindowUpdateFrame frame(kInvalidControlFrameId, bidirection_stream_id,
                              kDefaultFlowControlSendWindow * 2);
  pending.OnWindowUpdateFrame(frame);
  TestStream stream(&pending, session_.get(), false);

  EXPECT_EQ(QuicStreamPeer::SendWindowSize(&stream),
            kDefaultFlowControlSendWindow * 2);
}

TEST_P(PendingStreamTest, PendingStreamStopSending) {
  Initialize();

  QuicStreamId bidirection_stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      session_->transport_version(), Perspective::IS_CLIENT);
  PendingStream pending(bidirection_stream_id, session_.get());
  QuicResetStreamError error =
      QuicResetStreamError::FromInternal(QUIC_STREAM_INTERNAL_ERROR);
  pending.OnStopSending(error);
  EXPECT_TRUE(pending.GetStopSendingErrorCode());
  auto actual_error = *pending.GetStopSendingErrorCode();
  EXPECT_EQ(actual_error, error);
}

TEST_P(PendingStreamTest, FromPendingStream) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());

  QuicStreamFrame frame(kTestPendingStreamId, false, 2, ".");
  pending.OnStreamFrame(frame);
  pending.OnStreamFrame(frame);
  QuicStreamFrame frame2(kTestPendingStreamId, true, 3, ".");
  pending.OnStreamFrame(frame2);

  TestStream stream(&pending, session_.get(), false);
  EXPECT_EQ(3, stream.num_frames_received());
  EXPECT_EQ(3u, stream.stream_bytes_read());
  EXPECT_EQ(1, stream.num_duplicate_frames_received());
  EXPECT_EQ(true, stream.fin_received());
  EXPECT_EQ(frame2.offset + 1, stream.highest_received_byte_offset());
  EXPECT_EQ(frame2.offset + 1,
            session_->flow_controller()->highest_received_byte_offset());
}

TEST_P(PendingStreamTest, FromPendingStreamThenData) {
  Initialize();

  PendingStream pending(kTestPendingStreamId, session_.get());

  QuicStreamFrame frame(kTestPendingStreamId, false, 2, ".");
  pending.OnStreamFrame(frame);

  auto stream = new TestStream(&pending, session_.get(), false);
  session_->ActivateStream(absl::WrapUnique(stream));

  QuicStreamFrame frame2(kTestPendingStreamId, true, 3, ".");
  stream->OnStreamFrame(frame2);

  EXPECT_EQ(2, stream->num_frames_received());
  EXPECT_EQ(2u, stream->stream_bytes_read());
  EXPECT_EQ(true, stream->fin_received());
  EXPECT_EQ(frame2.offset + 1, stream->highest_received_byte_offset());
  EXPECT_EQ(frame2.offset + 1,
            session_->flow_controller()->highest_received_byte_offset());
}

TEST_P(PendingStreamTest, ResetStreamAt) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }

  PendingStream pending(kTestPendingStreamId, session_.get());

  QuicResetStreamAtFrame rst(0, kTestPendingStreamId, QUIC_STREAM_CANCELLED,
                             100, 3);
  pending.OnResetStreamAtFrame(rst);
  QuicStreamFrame frame(kTestPendingStreamId, false, 2, ".");
  pending.OnStreamFrame(frame);

  auto stream = new TestStream(&pending, session_.get(), false);
  session_->ActivateStream(absl::WrapUnique(stream));

  EXPECT_FALSE(stream->rst_received());
  EXPECT_FALSE(stream->read_side_closed());
  EXPECT_CALL(*stream, OnDataAvailable()).WillOnce([&]() {
    stream->ConsumeData(3);
  });
  QuicStreamFrame frame2(kTestPendingStreamId, false, 0, "..");
  stream->OnStreamFrame(frame2);
  EXPECT_TRUE(stream->read_side_closed());
  EXPECT_TRUE(stream->rst_received());
}

TEST_P(QuicStreamTest, WriteAllData) {
  Initialize();

  QuicByteCount length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), kPacket8ByteConnectionId,
              kPacket0ByteConnectionId, !kIncludeVersion,
              !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
              quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0,
              quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_P(QuicStreamTest, NoBlockingIfNoDataOrFin) {
  Initialize();

  // Write no data and no fin.  If we consume nothing we should not be write
  // blocked.
  EXPECT_QUIC_BUG(
      stream_->WriteOrBufferData(absl::string_view(), false, nullptr), "");
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_P(QuicStreamTest, BlockIfOnlySomeDataConsumed) {
  Initialize();

  // Write some data and no fin.  If we consume some but not all of the data,
  // we should be write blocked a not all the data was consumed.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 1u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 2), false, nullptr);
  EXPECT_TRUE(session_->HasUnackedStreamData());
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
}

TEST_P(QuicStreamTest, BlockIfFinNotConsumedWithData) {
  Initialize();

  // Write some data and no fin.  If we consume all the data but not the fin,
  // we should be write blocked because the fin was not consumed.
  // (This should never actually happen as the fin should be sent out with the
  // last data)
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 2u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 2), true, nullptr);
  EXPECT_TRUE(session_->HasUnackedStreamData());
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_P(QuicStreamTest, BlockIfSoloFinNotConsumed) {
  Initialize();

  // Write no data and a fin.  If we consume nothing we should be write blocked,
  // as the fin was not consumed.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(absl::string_view(), true, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_P(QuicStreamTest, CloseOnPartialWrite) {
  Initialize();

  // Write some data and no fin. However, while writing the data
  // close the stream and verify that MarkConnectionLevelWriteBlocked does not
  // crash with an unknown stream.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(Invoke(this, &QuicStreamTest::CloseStreamOnWriteError));
  stream_->WriteOrBufferData(absl::string_view(kData1, 2), false, nullptr);
  ASSERT_EQ(0u, write_blocked_list_->NumBlockedStreams());
}

TEST_P(QuicStreamTest, WriteOrBufferData) {
  Initialize();

  EXPECT_FALSE(HasWriteBlockedStreams());
  QuicByteCount length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), kPacket8ByteConnectionId,
              kPacket0ByteConnectionId, !kIncludeVersion,
              !kIncludeDiversificationNonce, PACKET_4BYTE_PACKET_NUMBER,
              quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0,
              quiche::VARIABLE_LENGTH_INTEGER_LENGTH_0, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), kDataLen - 1, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(kData1, false, nullptr);

  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_EQ(10u, stream_->BufferedDataBytes());
  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), kDataLen - 1, kDataLen - 1,
                                     NO_FIN, NOT_RETRANSMISSION, std::nullopt);
      }));
  EXPECT_CALL(*stream_, OnCanWriteNewData());
  stream_->OnCanWrite();
  EXPECT_TRUE(session_->HasUnackedStreamData());

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 2u, 2 * kDataLen - 2,
                                     NO_FIN, NOT_RETRANSMISSION, std::nullopt);
      }));
  EXPECT_CALL(*stream_, OnCanWriteNewData());
  stream_->OnCanWrite();
  EXPECT_TRUE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, WriteOrBufferDataReachStreamLimit) {
  Initialize();
  std::string data("aaaaa");
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - data.length(),
                                        stream_);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
        stream_->WriteOrBufferData("a", false, nullptr);
      },
      "Write too many data via stream");
}

TEST_P(QuicStreamTest, ConnectionCloseAfterStreamClose) {
  Initialize();

  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(session_->transport_version())) {
    // Create and inject a STOP SENDING frame to complete the close
    // of the stream. This is only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId, stream_->id(),
                                      QUIC_STREAM_CANCELLED);
    session_->OnStopSendingFrame(stop_sending);
  }
  EXPECT_THAT(stream_->stream_error(), IsStreamError(QUIC_STREAM_CANCELLED));
  EXPECT_THAT(stream_->connection_error(), IsQuicNoError());
  QuicConnectionCloseFrame frame;
  frame.quic_error_code = QUIC_INTERNAL_ERROR;
  stream_->OnConnectionClosed(frame, ConnectionCloseSource::FROM_SELF);
  EXPECT_THAT(stream_->stream_error(), IsStreamError(QUIC_STREAM_CANCELLED));
  EXPECT_THAT(stream_->connection_error(), IsQuicNoError());
}

TEST_P(QuicStreamTest, RstAlwaysSentIfNoFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if no FIN has been sent, we send a RST.

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with no FIN.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 1u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 1), false, nullptr);
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we send a RST.
  EXPECT_CALL(*session_, MaybeSendRstStreamFrame(kTestStreamId, _, _));
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(session_->transport_version())) {
    // Create and inject a STOP SENDING frame to complete the close
    // of the stream. This is only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId, stream_->id(),
                                      QUIC_STREAM_CANCELLED);
    session_->OnStopSendingFrame(stop_sending);
  }
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_P(QuicStreamTest, RstNotSentIfFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a FIN has been sent, we don't also send a RST.

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with FIN.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 1u, 0u, FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 1), true, nullptr);
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we do not send a RST.
  QuicStreamPeer::CloseReadSide(stream_);
  stream_->CloseWriteSide();
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());
}

TEST_P(QuicStreamTest, OnlySendOneRst) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a stream sends a RST, it doesn't send an additional RST during
  // OnClose() (this shouldn't be harmful, but we shouldn't do it anyway...)

  Initialize();
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Reset the stream.
  EXPECT_CALL(*session_, MaybeSendRstStreamFrame(kTestStreamId, _, _)).Times(1);
  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());

  // Now close the stream (any further resets being sent would break the
  // expectation above).
  QuicStreamPeer::CloseReadSide(stream_);
  stream_->CloseWriteSide();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_P(QuicStreamTest, StreamFlowControlMultipleWindowUpdates) {
  Initialize();

  // If we receive multiple WINDOW_UPDATES (potentially out of order), then we
  // want to make sure we latch the largest offset we see.

  // Initially should be default.
  EXPECT_EQ(kMinimumFlowControlSendWindow,
            QuicStreamPeer::SendWindowOffset(stream_));

  // Check a single WINDOW_UPDATE results in correct offset.
  QuicWindowUpdateFrame window_update_1(kInvalidControlFrameId, stream_->id(),
                                        kMinimumFlowControlSendWindow + 5);
  stream_->OnWindowUpdateFrame(window_update_1);
  EXPECT_EQ(window_update_1.max_data,
            QuicStreamPeer::SendWindowOffset(stream_));

  // Now send a few more WINDOW_UPDATES and make sure that only the largest is
  // remembered.
  QuicWindowUpdateFrame window_update_2(kInvalidControlFrameId, stream_->id(),
                                        1);
  QuicWindowUpdateFrame window_update_3(kInvalidControlFrameId, stream_->id(),
                                        kMinimumFlowControlSendWindow + 10);
  QuicWindowUpdateFrame window_update_4(kInvalidControlFrameId, stream_->id(),
                                        5678);
  stream_->OnWindowUpdateFrame(window_update_2);
  stream_->OnWindowUpdateFrame(window_update_3);
  stream_->OnWindowUpdateFrame(window_update_4);
  EXPECT_EQ(window_update_3.max_data,
            QuicStreamPeer::SendWindowOffset(stream_));
}

TEST_P(QuicStreamTest, FrameStats) {
  Initialize();

  EXPECT_EQ(0, stream_->num_frames_received());
  EXPECT_EQ(0, stream_->num_duplicate_frames_received());
  QuicStreamFrame frame(stream_->id(), false, 0, ".");
  EXPECT_CALL(*stream_, OnDataAvailable()).Times(2);
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(1, stream_->num_frames_received());
  EXPECT_EQ(0, stream_->num_duplicate_frames_received());
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(2, stream_->num_frames_received());
  EXPECT_EQ(1, stream_->num_duplicate_frames_received());
  QuicStreamFrame frame2(stream_->id(), false, 1, "abc");
  stream_->OnStreamFrame(frame2);
}

// Verify that when we receive a packet which violates flow control (i.e. sends
// too much data on the stream) that the stream sequencer never sees this frame,
// as we check for violation and close the connection early.
TEST_P(QuicStreamTest, StreamSequencerNeverSeesPacketsViolatingFlowControl) {
  Initialize();

  // Receive a stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicStreamFrame frame(stream_->id(), false,
                        kInitialSessionFlowControlWindowForTest + 1, ".");
  EXPECT_GT(frame.offset, QuicStreamPeer::ReceiveWindowOffset(stream_));

  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamFrame(frame);
}

// Verify that after the consumer calls StopReading(), the stream still sends
// flow control updates.
TEST_P(QuicStreamTest, StopReadingSendsFlowControl) {
  Initialize();

  stream_->StopReading();

  // Connection should not get terminated due to flow control errors.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _))
      .Times(0);
  EXPECT_CALL(*session_, WriteControlFrame(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&ClearControlFrameWithTransmissionType));

  std::string data(1000, 'x');
  for (QuicStreamOffset offset = 0;
       offset < 2 * kInitialStreamFlowControlWindowForTest;
       offset += data.length()) {
    QuicStreamFrame frame(stream_->id(), false, offset, data);
    stream_->OnStreamFrame(frame);
  }
  EXPECT_LT(kInitialStreamFlowControlWindowForTest,
            QuicStreamPeer::ReceiveWindowOffset(stream_));
}

TEST_P(QuicStreamTest, FinalByteOffsetFromFin) {
  Initialize();

  EXPECT_FALSE(stream_->HasReceivedFinalOffset());

  QuicStreamFrame stream_frame_no_fin(stream_->id(), false, 1234, ".");
  stream_->OnStreamFrame(stream_frame_no_fin);
  EXPECT_FALSE(stream_->HasReceivedFinalOffset());

  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234, ".");
  stream_->OnStreamFrame(stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
}

TEST_P(QuicStreamTest, FinalByteOffsetFromRst) {
  Initialize();

  EXPECT_FALSE(stream_->HasReceivedFinalOffset());
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
}

TEST_P(QuicStreamTest, InvalidFinalByteOffsetFromRst) {
  Initialize();

  EXPECT_FALSE(stream_->HasReceivedFinalOffset());
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 0xFFFFFFFFFFFF);
  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
}

TEST_P(QuicStreamTest, FinalByteOffsetFromZeroLengthStreamFrame) {
  // When receiving Trailers, an empty stream frame is created with the FIN set,
  // and is passed to OnStreamFrame. The Trailers may be sent in advance of
  // queued body bytes being sent, and thus the final byte offset may exceed
  // current flow control limits. Flow control should only be concerned with
  // data that has actually been sent/received, so verify that flow control
  // ignores such a stream frame.
  Initialize();

  EXPECT_FALSE(stream_->HasReceivedFinalOffset());
  const QuicStreamOffset kByteOffsetExceedingFlowControlWindow =
      kInitialSessionFlowControlWindowForTest + 1;
  const QuicStreamOffset current_stream_flow_control_offset =
      QuicStreamPeer::ReceiveWindowOffset(stream_);
  const QuicStreamOffset current_connection_flow_control_offset =
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller());
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_stream_flow_control_offset);
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_connection_flow_control_offset);
  QuicStreamFrame zero_length_stream_frame_with_fin(
      stream_->id(), /*fin=*/true, kByteOffsetExceedingFlowControlWindow,
      absl::string_view());
  EXPECT_EQ(0, zero_length_stream_frame_with_fin.data_length);

  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  stream_->OnStreamFrame(zero_length_stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());

  // The flow control receive offset values should not have changed.
  EXPECT_EQ(current_stream_flow_control_offset,
            QuicStreamPeer::ReceiveWindowOffset(stream_));
  EXPECT_EQ(
      current_connection_flow_control_offset,
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller()));
}

TEST_P(QuicStreamTest, OnStreamResetOffsetOverflow) {
  Initialize();
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, kMaxStreamLength + 1);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
  stream_->OnStreamReset(rst_frame);
}

TEST_P(QuicStreamTest, OnStreamFrameUpperLimit) {
  Initialize();

  // Modify receive window offset and sequencer buffer total_bytes_read_ to
  // avoid flow control violation.
  QuicStreamPeer::SetReceiveWindowOffset(stream_, kMaxStreamLength + 5u);
  QuicFlowControllerPeer::SetReceiveWindowOffset(session_->flow_controller(),
                                                 kMaxStreamLength + 5u);
  QuicStreamSequencerPeer::SetFrameBufferTotalBytesRead(
      QuicStreamPeer::sequencer(stream_), kMaxStreamLength - 10u);

  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _))
      .Times(0);
  QuicStreamFrame stream_frame(stream_->id(), false, kMaxStreamLength - 1, ".");
  stream_->OnStreamFrame(stream_frame);
  QuicStreamFrame stream_frame2(stream_->id(), true, kMaxStreamLength, "");
  stream_->OnStreamFrame(stream_frame2);
}

TEST_P(QuicStreamTest, StreamTooLong) {
  Initialize();
  QuicStreamFrame stream_frame(stream_->id(), false, kMaxStreamLength, ".");
  EXPECT_QUIC_PEER_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _))
            .Times(1);
        stream_->OnStreamFrame(stream_frame);
      },
      absl::StrCat("Receive stream frame on stream ", stream_->id(),
                   " reaches max stream length"));
}

TEST_P(QuicStreamTest, SetDrainingIncomingOutgoing) {
  // Don't have incoming data consumed.
  Initialize();

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234, ".");
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, QuicSessionPeer::GetNumOpenDynamicStreams(session_.get()));

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 2u, 0u, FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, QuicSessionPeer::GetNumDrainingStreams(session_.get()));
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(session_.get()));
}

TEST_P(QuicStreamTest, SetDrainingOutgoingIncoming) {
  // Don't have incoming data consumed.
  Initialize();

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(kTestStreamId, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 2u, 0u, FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(absl::string_view(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, QuicSessionPeer::GetNumOpenDynamicStreams(session_.get()));

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234, ".");
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, QuicSessionPeer::GetNumDrainingStreams(session_.get()));
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(session_.get()));
}

TEST_P(QuicStreamTest, EarlyResponseFinHandling) {
  // Verify that if the server completes the response before reading the end of
  // the request, the received FIN is recorded.

  Initialize();
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));

  // Receive data for the request.
  EXPECT_CALL(*stream_, OnDataAvailable()).Times(1);
  QuicStreamFrame frame1(stream_->id(), false, 0, "Start");
  stream_->OnStreamFrame(frame1);
  // When QuicSimpleServerStream sends the response, it calls
  // QuicStream::CloseReadSide() first.
  QuicStreamPeer::CloseReadSide(stream_);
  // Send data and FIN for the response.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  // Receive remaining data and FIN for the request.
  QuicStreamFrame frame2(stream_->id(), true, 0, "End");
  stream_->OnStreamFrame(frame2);
  EXPECT_TRUE(stream_->fin_received());
  EXPECT_TRUE(stream_->HasReceivedFinalOffset());
}

TEST_P(QuicStreamTest, StreamWaitsForAcks) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  // Stream is not waiting for acks initially.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(session_->HasUnackedStreamData());

  // Send kData1.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(9u, newly_acked_length);
  // Stream is not waiting for acks as all sent data is acked.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData2.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Send FIN.
  stream_->WriteOrBufferData("", true, nullptr);
  // Fin only frame is not stored in send buffer.
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // kData2 is retransmitted.
  stream_->OnStreamFrameRetransmitted(9, 9, false);

  // kData2 is acked.
  EXPECT_TRUE(stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(9u, newly_acked_length);
  // Stream is waiting for acks as FIN is not acked.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // FIN is acked.
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState());
  EXPECT_TRUE(stream_->OnStreamFrameAcked(18, 0, true, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(0u, newly_acked_length);
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_P(QuicStreamTest, StreamDataGetAckedOutOfOrder) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  // Send data.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData("", true, nullptr);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(9u, newly_acked_length);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->OnStreamFrameAcked(18, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(9u, newly_acked_length);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(9u, newly_acked_length);
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  // FIN is not acked yet.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState());
  EXPECT_TRUE(stream_->OnStreamFrameAcked(27, 0, true, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(0u, newly_acked_length);
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, CancelStream) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Cancel stream.
  stream_->MaybeSendStopSending(QUIC_STREAM_NO_ERROR);
  // stream still waits for acks as the error code is QUIC_STREAM_NO_ERROR, and
  // data is going to be retransmitted.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_STREAM_CANCELLED));
  EXPECT_CALL(*session_, WriteControlFrame(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke(&ClearControlFrameWithTransmissionType));

  EXPECT_CALL(*session_, MaybeSendRstStreamFrame(_, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        session_->ReallyMaybeSendRstStreamFrame(
            stream_->id(), QUIC_STREAM_CANCELLED,
            stream_->stream_bytes_written());
      }));

  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as data is not going to be retransmitted.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, RstFrameReceivedStreamNotFinishSending) {
  if (VersionHasIetfQuicFrames(GetParam().transport_version)) {
    // In IETF QUIC, receiving a RESET_STREAM will only close the read side. The
    // stream itself is not closed and will not send reset.
    return;
  }

  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // RST_STREAM received.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 9);

  EXPECT_CALL(
      *session_,
      MaybeSendRstStreamFrame(
          stream_->id(),
          QuicResetStreamError::FromInternal(QUIC_RST_ACKNOWLEDGEMENT), 9));
  stream_->OnStreamReset(rst_frame);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as it does not finish sending and rst is
  // sent.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, RstFrameReceivedStreamFinishSending) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, true, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());

  // RST_STREAM received.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  // Stream still waits for acks as it finishes sending and has unacked data.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_P(QuicStreamTest, ConnectionClosed) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  EXPECT_CALL(
      *session_,
      MaybeSendRstStreamFrame(
          stream_->id(),
          QuicResetStreamError::FromInternal(QUIC_RST_ACKNOWLEDGEMENT), 9));
  QuicConnectionPeer::SetConnectionClose(connection_);
  QuicConnectionCloseFrame frame;
  frame.quic_error_code = QUIC_INTERNAL_ERROR;
  stream_->OnConnectionClosed(frame, ConnectionCloseSource::FROM_SELF);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Stream stops waiting for acks as connection is going to close.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, CanWriteNewDataAfterData) {
  SetQuicFlag(quic_buffered_data_threshold, 100);
  Initialize();
  EXPECT_TRUE(stream_->CanWriteNewDataAfterData(99));
  EXPECT_FALSE(stream_->CanWriteNewDataAfterData(100));
}

TEST_P(QuicStreamTest, WriteBufferedData) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(quic_buffered_data_threshold, 100);

  Initialize();
  std::string data(1024, 'a');
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing WriteOrBufferData.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 100u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Verify all data is saved.
  EXPECT_EQ(3 * data.length() - 100, stream_->BufferedDataBytes());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 100, 100u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  // Buffered data size > threshold, do not ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  stream_->OnCanWrite();
  EXPECT_EQ(3 * data.length() - 200, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  // Send buffered data to make buffered data size < threshold.
  QuicByteCount data_to_write =
      3 * data.length() - 200 - GetQuicFlag(quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return session_->ConsumeData(stream_->id(), data_to_write, 200u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  // Buffered data size < threshold, ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(
      static_cast<uint64_t>(GetQuicFlag(quic_buffered_data_threshold) - 1),
      stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(0u, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing Writev.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  QuicConsumedData consumed = SendApplicationData(data, data.length(), false);

  // There is no buffered data before, all data should be consumed without
  // respecting buffered data upper limit.
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  consumed = SendApplicationData(data, data.length(), false);

  // No Data can be consumed as buffered data is beyond upper limit.
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());

  data_to_write = data.length() - GetQuicFlag(quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return session_->ConsumeData(stream_->id(), data_to_write, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));

  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(
      static_cast<uint64_t>(GetQuicFlag(quic_buffered_data_threshold) - 1),
      stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  // All data can be consumed as buffered data is below upper limit.
  consumed = SendApplicationData(data, data.length(), false);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length() + GetQuicFlag(quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());
}

TEST_P(QuicStreamTest, WritevDataReachStreamLimit) {
  Initialize();
  std::string data("aaaaa");
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - data.length(),
                                        stream_);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  QuicConsumedData consumed = SendApplicationData(data, 5, false);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
        SendApplicationData(data, 1, false);
      },
      "Write too many data via stream");
}

TEST_P(QuicStreamTest, WriteMemSlices) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(quic_buffered_data_threshold, 100);

  Initialize();
  constexpr QuicByteCount kDataSize = 1024;
  quiche::QuicheBufferAllocator* allocator =
      connection_->helper()->GetStreamSendBufferAllocator();
  std::vector<quiche::QuicheMemSlice> vector1;
  vector1.push_back(
      quiche::QuicheMemSlice(quiche::QuicheBuffer(allocator, kDataSize)));
  vector1.push_back(
      quiche::QuicheMemSlice(quiche::QuicheBuffer(allocator, kDataSize)));
  std::vector<quiche::QuicheMemSlice> vector2;
  vector2.push_back(
      quiche::QuicheMemSlice(quiche::QuicheBuffer(allocator, kDataSize)));
  vector2.push_back(
      quiche::QuicheMemSlice(quiche::QuicheBuffer(allocator, kDataSize)));
  absl::Span<quiche::QuicheMemSlice> span1(vector1);
  absl::Span<quiche::QuicheMemSlice> span2(vector2);

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 100u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  // There is no buffered data before, all data should be consumed.
  QuicConsumedData consumed = stream_->WriteMemSlices(span1, false);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * kDataSize - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  // No Data can be consumed as buffered data is beyond upper limit.
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * kDataSize - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  QuicByteCount data_to_write =
      2 * kDataSize - 100 - GetQuicFlag(quic_buffered_data_threshold) + 1;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this, data_to_write]() {
        return session_->ConsumeData(stream_->id(), data_to_write, 100u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(
      static_cast<uint64_t>(GetQuicFlag(quic_buffered_data_threshold) - 1),
      stream_->BufferedDataBytes());
  // Try to write slices2 again.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_EQ(2 * kDataSize + GetQuicFlag(quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->fin_buffered());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicStreamTest, WriteMemSlicesReachStreamLimit) {
  Initialize();
  QuicStreamPeer::SetStreamBytesWritten(kMaxStreamLength - 5u, stream_);
  std::vector<std::pair<char*, size_t>> buffers;
  quiche::QuicheMemSlice slice1 = MemSliceFromString("12345");
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 5u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  // There is no buffered data before, all data should be consumed.
  QuicConsumedData consumed = stream_->WriteMemSlice(std::move(slice1), false);
  EXPECT_EQ(5u, consumed.bytes_consumed);

  quiche::QuicheMemSlice slice2 = MemSliceFromString("6");
  EXPECT_QUIC_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_STREAM_LENGTH_OVERFLOW, _, _));
        stream_->WriteMemSlice(std::move(slice2), false);
      },
      "Write too many data via stream");
}

TEST_P(QuicStreamTest, StreamDataGetAckedMultipleTimes) {
  Initialize();
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());

  // Send [0, 27) and fin.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, true, nullptr);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());
  // Ack [0, 9), [5, 22) and [18, 26)
  // Verify [0, 9) 9 bytes are acked.
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(9u, newly_acked_length);
  EXPECT_EQ(2u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [9, 22) 13 bytes are acked.
  EXPECT_TRUE(stream_->OnStreamFrameAcked(5, 17, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(13u, newly_acked_length);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [22, 26) 4 bytes are acked.
  EXPECT_TRUE(stream_->OnStreamFrameAcked(18, 8, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(4u, newly_acked_length);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());

  // Ack [0, 27). Verify [26, 27) 1 byte is acked.
  EXPECT_TRUE(stream_->OnStreamFrameAcked(26, 1, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(1u, newly_acked_length);
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_->HasUnackedStreamData());

  // Ack Fin.
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_TRUE(stream_->OnStreamFrameAcked(27, 0, true, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(0u, newly_acked_length);
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());

  // Ack [10, 27) and fin. No new data is acked.
  EXPECT_FALSE(stream_->OnStreamFrameAcked(
      10, 17, true, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(0u, newly_acked_length);
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_FALSE(session_->HasUnackedStreamData());
}

TEST_P(QuicStreamTest, OnStreamFrameLost) {
  Initialize();

  // Send [0, 9).
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->IsStreamFrameOutstanding(0, 9, false));

  // Try to send [9, 27), but connection is blocked.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(kData2, false, nullptr);
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_TRUE(stream_->HasBufferedData());
  EXPECT_FALSE(stream_->HasPendingRetransmission());

  // Lost [0, 9). When stream gets a chance to write, only lost data is
  // transmitted.
  stream_->OnStreamFrameLost(0, 9, false);
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  EXPECT_TRUE(stream_->HasBufferedData());

  // This OnCanWrite causes [9, 27) to be sent.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasBufferedData());

  // Send a fin only frame.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData("", true, nullptr);

  // Lost [9, 27) and fin.
  stream_->OnStreamFrameLost(9, 18, false);
  stream_->OnStreamFrameLost(27, 0, true);
  EXPECT_TRUE(stream_->HasPendingRetransmission());

  // Ack [9, 18).
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero(),
                                          QuicTime::Zero(), &newly_acked_length,
                                          /*is_retransmission=*/false));
  EXPECT_EQ(9u, newly_acked_length);
  EXPECT_FALSE(stream_->IsStreamFrameOutstanding(9, 3, false));
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  // This OnCanWrite causes [18, 27) and fin to be retransmitted. Verify fin can
  // be bundled with data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 9u, 18u, FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  // Lost [9, 18) again, but it is not considered as lost because kData2
  // has been acked.
  stream_->OnStreamFrameLost(9, 9, false);
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  EXPECT_TRUE(stream_->IsStreamFrameOutstanding(27, 0, true));
}

TEST_P(QuicStreamTest, CannotBundleLostFin) {
  Initialize();

  // Send [0, 18) and fin.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData2, true, nullptr);

  // Lost [0, 9) and fin.
  stream_->OnStreamFrameLost(0, 9, false);
  stream_->OnStreamFrameLost(18, 0, true);

  // Retransmit lost data. Verify [0, 9) and fin are retransmitted in two
  // frames.
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 9u, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, true)));
  stream_->OnCanWrite();
}

TEST_P(QuicStreamTest, MarkConnectionLevelWriteBlockedOnWindowUpdateFrame) {
  Initialize();

  // Set the config to a small value so that a newly created stream has small
  // send flow control window.
  QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(session_->config(),
                                                            100);
  QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesIncomingBidirectional(
      session_->config(), 100);
  auto stream = new TestStream(GetNthClientInitiatedBidirectionalStreamId(
                                   GetParam().transport_version, 2),
                               session_.get(), BIDIRECTIONAL);
  session_->ActivateStream(absl::WrapUnique(stream));

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, SendBlocked(_, _)).Times(1);
  std::string data(1024, '.');
  stream->WriteOrBufferData(data, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());

  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream_->id(),
                                      1234);

  stream->OnWindowUpdateFrame(window_update);
  // Verify stream is marked connection level write blocked.
  EXPECT_TRUE(HasWriteBlockedStreams());
  EXPECT_TRUE(stream->HasBufferedData());
}

// Regression test for b/73282665.
TEST_P(QuicStreamTest,
       MarkConnectionLevelWriteBlockedOnWindowUpdateFrameWithNoBufferedData) {
  Initialize();

  // Set the config to a small value so that a newly created stream has small
  // send flow control window.
  QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(session_->config(),
                                                            100);
  QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesIncomingBidirectional(
      session_->config(), 100);
  auto stream = new TestStream(GetNthClientInitiatedBidirectionalStreamId(
                                   GetParam().transport_version, 2),
                               session_.get(), BIDIRECTIONAL);
  session_->ActivateStream(absl::WrapUnique(stream));

  std::string data(100, '.');
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, SendBlocked(_, _)).Times(1);
  stream->WriteOrBufferData(data, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());

  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream_->id(),
                                      120);
  stream->OnWindowUpdateFrame(window_update);
  EXPECT_FALSE(stream->HasBufferedData());
  // Verify stream is marked as blocked although there is no buffered data.
  EXPECT_TRUE(HasWriteBlockedStreams());
}

TEST_P(QuicStreamTest, RetransmitStreamData) {
  Initialize();
  InSequence s;

  // Send [0, 18) with fin.
  EXPECT_CALL(*session_, WritevData(stream_->id(), _, _, _, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, true, nullptr);
  // Ack [10, 13).
  QuicByteCount newly_acked_length = 0;
  stream_->OnStreamFrameAcked(10, 3, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  EXPECT_EQ(3u, newly_acked_length);
  // Retransmit [0, 18) with fin, and only [0, 8) is consumed.
  EXPECT_CALL(*session_, WritevData(stream_->id(), 10, 0, NO_FIN, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_->ConsumeData(stream_->id(), 8, 0u, NO_FIN,
                                     NOT_RETRANSMISSION, std::nullopt);
      }));
  EXPECT_FALSE(stream_->RetransmitStreamData(0, 18, true, PTO_RETRANSMISSION));

  // Retransmit [0, 18) with fin, and all is consumed.
  EXPECT_CALL(*session_, WritevData(stream_->id(), 10, 0, NO_FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, WritevData(stream_->id(), 5, 13, FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 18, true, PTO_RETRANSMISSION));

  // Retransmit [0, 8) with fin, and all is consumed.
  EXPECT_CALL(*session_, WritevData(stream_->id(), 8, 0, NO_FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_CALL(*session_, WritevData(stream_->id(), 0, 18, FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 8, true, PTO_RETRANSMISSION));
}

TEST_P(QuicStreamTest, ResetStreamOnTtlExpiresRetransmitLostData) {
  Initialize();

  EXPECT_CALL(*session_, WritevData(stream_->id(), 200, 0, FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  std::string body(200, 'a');
  stream_->WriteOrBufferData(body, true, nullptr);

  // Set TTL to be 1 s.
  QuicTime::Delta ttl = QuicTime::Delta::FromSeconds(1);
  ASSERT_TRUE(stream_->MaybeSetTtl(ttl));
  // Verify data gets retransmitted because TTL does not expire.
  EXPECT_CALL(*session_, WritevData(stream_->id(), 100, 0, NO_FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(0, 100, false, PTO_RETRANSMISSION));
  stream_->OnStreamFrameLost(100, 100, true);
  EXPECT_TRUE(stream_->HasPendingRetransmission());

  connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  // Verify stream gets reset because TTL expires.
  if (session_->version().UsesHttp3()) {
    EXPECT_CALL(*session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_TTL_EXPIRED)))
        .Times(1);
  }
  EXPECT_CALL(
      *session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_STREAM_TTL_EXPIRED), _))
      .Times(1);
  stream_->OnCanWrite();
}

TEST_P(QuicStreamTest, ResetStreamOnTtlExpiresEarlyRetransmitData) {
  Initialize();

  EXPECT_CALL(*session_, WritevData(stream_->id(), 200, 0, FIN, _, _))
      .WillOnce(Invoke(session_.get(), &MockQuicSession::ConsumeData));
  std::string body(200, 'a');
  stream_->WriteOrBufferData(body, true, nullptr);

  // Set TTL to be 1 s.
  QuicTime::Delta ttl = QuicTime::Delta::FromSeconds(1);
  ASSERT_TRUE(stream_->MaybeSetTtl(ttl));

  connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
  // Verify stream gets reset because TTL expires.
  if (session_->version().UsesHttp3()) {
    EXPECT_CALL(*session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_TTL_EXPIRED)))
        .Times(1);
  }
  EXPECT_CALL(
      *session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_STREAM_TTL_EXPIRED), _))
      .Times(1);
  stream_->RetransmitStreamData(0, 100, false, PTO_RETRANSMISSION);
}

// Test that OnStreamReset does one-way (read) closes if version 99, two way
// (read and write) if not version 99.
TEST_P(QuicStreamTest, OnStreamResetReadOrReadWrite) {
  Initialize();
  EXPECT_FALSE(stream_->write_side_closed());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));

  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(connection_->transport_version())) {
    // Version 99/IETF QUIC should close just the read side.
    EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
    EXPECT_FALSE(stream_->write_side_closed());
  } else {
    // Google QUIC should close both sides of the stream.
    EXPECT_TRUE(stream_->write_side_closed());
    EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  }
}

TEST_P(QuicStreamTest, WindowUpdateForReadOnlyStream) {
  Initialize();

  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      connection_->transport_version(), Perspective::IS_CLIENT);
  TestStream stream(stream_id, session_.get(), READ_UNIDIRECTIONAL);
  QuicWindowUpdateFrame window_update_frame(kInvalidControlFrameId, stream_id,
                                            0);
  EXPECT_CALL(
      *connection_,
      CloseConnection(
          QUIC_WINDOW_UPDATE_RECEIVED_ON_READ_UNIDIRECTIONAL_STREAM,
          "WindowUpdateFrame received on READ_UNIDIRECTIONAL stream.", _));
  stream.OnWindowUpdateFrame(window_update_frame);
}

TEST_P(QuicStreamTest, RstStreamFrameChangesCloseOffset) {
  Initialize();

  QuicStreamFrame stream_frame(stream_->id(), true, 0, "abc");
  EXPECT_CALL(*stream_, OnDataAvailable());
  stream_->OnStreamFrame(stream_frame);
  QuicRstStreamFrame rst(kInvalidControlFrameId, stream_->id(),
                         QUIC_STREAM_CANCELLED, 0u);

  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_MULTIPLE_OFFSET, _, _));
  stream_->OnStreamReset(rst);
}

// Regression test for b/176073284.
TEST_P(QuicStreamTest, EmptyStreamFrameWithNoFin) {
  Initialize();
  QuicStreamFrame empty_stream_frame(stream_->id(), false, 0, "");
  if (stream_->version().HasIetfQuicFrames()) {
    EXPECT_CALL(*connection_,
                CloseConnection(QUIC_EMPTY_STREAM_FRAME_NO_FIN, _, _))
        .Times(0);
  } else {
    EXPECT_CALL(*connection_,
                CloseConnection(QUIC_EMPTY_STREAM_FRAME_NO_FIN, _, _));
  }
  EXPECT_CALL(*stream_, OnDataAvailable()).Times(0);
  stream_->OnStreamFrame(empty_stream_frame);
}

TEST_P(QuicStreamTest, SendRstWithCustomIetfCode) {
  Initialize();
  QuicResetStreamError error(QUIC_STREAM_CANCELLED, 0x1234abcd);
  EXPECT_CALL(*session_, MaybeSendRstStreamFrame(kTestStreamId, error, _))
      .Times(1);
  stream_->ResetWithError(error);
  EXPECT_TRUE(rst_sent());
}

TEST_P(QuicStreamTest, ResetWhenOffsetReached) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 400, 100);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.

  // Send data to reach reliable_offset.
  char data[100];
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(99);
  });
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), false, 0, absl::string_view(data, 99)));
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_FALSE(stream_->read_side_closed());
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(1);
  });
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), false, 99,
                                         absl::string_view(data + 99, 1)));
  EXPECT_TRUE(stream_->rst_received());
  EXPECT_TRUE(stream_->read_side_closed());
}

TEST_P(QuicStreamTest, ResetWhenOffsetReachedOutOfOrder) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 400, 100);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.

  // Send data to reach reliable_offset.
  char data[100];
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), false, 99,
                                         absl::string_view(data + 99, 1)));
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_FALSE(stream_->read_side_closed());
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(100);
  });
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), false, 0, absl::string_view(data, 99)));
  EXPECT_TRUE(stream_->rst_received());
  EXPECT_TRUE(stream_->read_side_closed());
}

TEST_P(QuicStreamTest, HigherReliableSizeIgnored) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 400, 100);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.
  QuicResetStreamAtFrame rst2(0, stream_->id(), QUIC_STREAM_CANCELLED, 400,
                              200);
  stream_->OnResetStreamAtFrame(rst2);  // Ignored.

  // Send data to reach reliable_offset.
  char data[100];
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(99);
  });
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), false, 0, absl::string_view(data, 99)));
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_FALSE(stream_->read_side_closed());
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(1);
  });
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), false, 99,
                                         absl::string_view(data + 99, 1)));
  EXPECT_TRUE(stream_->rst_received());
  EXPECT_TRUE(stream_->read_side_closed());
}

TEST_P(QuicStreamTest, InstantReset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(100);
  });
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), false, 0, absl::string_view(data, 100)));
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 400, 100);
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_FALSE(stream_->read_side_closed());
  stream_->OnResetStreamAtFrame(rst);
  EXPECT_TRUE(stream_->rst_received());
  EXPECT_TRUE(stream_->read_side_closed());
}

TEST_P(QuicStreamTest, ResetIgnoredDueToFin) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(98);
  });
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), false, 0, absl::string_view(data, 98)));
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 100, 99);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.
  // There is no call to OnFinRead() because the stream is responsible for
  // doing that.
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_FALSE(stream_->read_side_closed());
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([this]() {
    stream_->ConsumeData(2);
    stream_->OnFinRead();
  });
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), true, 98,
                                         absl::string_view(data + 98, 2)));
  EXPECT_FALSE(stream_->rst_received());
  EXPECT_TRUE(stream_->read_side_closed());
}

TEST_P(QuicStreamTest, ReliableOffsetBeyondFin) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), true, 98,
                                         absl::string_view(data + 98, 2)));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_MULTIPLE_OFFSET, _, _))
      .Times(1);
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 101, 101);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.
}

TEST_P(QuicStreamTest, FinBeforeReliableOffset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  QuicResetStreamAtFrame rst(0, stream_->id(), QUIC_STREAM_CANCELLED, 101, 101);
  stream_->OnResetStreamAtFrame(rst);  // Nothing happens.
  char data[100];
  EXPECT_CALL(*connection_, CloseConnection(QUIC_STREAM_MULTIPLE_OFFSET, _, _))
      .Times(1);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), true, 0, absl::string_view(data, 100)));
}

TEST_P(QuicStreamTest, ReliableSizeNotAckedAtTimeOfReset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  EXPECT_TRUE(closed_streams->empty());
  // Peer sends RST_STREAM in response.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_EQ((*(closed_streams->begin()))->id(), stream_->id());
  ASSERT_EQ(closed_streams->size(), 1);
}

TEST_P(QuicStreamTest, ReliableSizeNotAckedAtTimeOfResetAndRetransmitted) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_TRUE(stream_->SetReliableSize());
  // Send 50 more bytes that aren't reliable.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(50, false)));
  SendApplicationData(data, 50, false);
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));

  // Lose all the bytes.
  stream_->OnStreamFrameLost(0, 150, false);
  // Cause retransmission of the reliable bytes.
  EXPECT_CALL(*session_, WritevData(stream_->id(), 100, 0, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  stream_->OnCanWrite();

  // Ack the reliable bytes, and close.
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  EXPECT_TRUE(closed_streams->empty());
  // Peer sends RST_STREAM in response.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_EQ((*(closed_streams->begin()))->id(), stream_->id());
  ASSERT_EQ(closed_streams->size(), 1);
}

TEST_P(QuicStreamTest, ReliableSizeNotAckedAtTimeOfResetThenReadSideReset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  EXPECT_TRUE(stream_->SetReliableSize());
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));

  // Peer sends RST_STREAM in response.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  ASSERT_TRUE(closed_streams->empty());
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  ASSERT_EQ(closed_streams->size(), 1);
  EXPECT_EQ((*(closed_streams->begin()))->id(), stream_->id());
}

TEST_P(QuicStreamTest, ReliableSizeNotAckedAtTimeOfResetThenReadSideFin) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  EXPECT_TRUE(stream_->SetReliableSize());
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  EXPECT_TRUE(stream_->write_side_closed());

  // Peer sends OOO FIN.
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), true, sizeof(data), ""));
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  ASSERT_TRUE(closed_streams->empty());
  EXPECT_FALSE(stream_->read_side_closed());  // Missing the data before 100.

  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  ASSERT_TRUE(closed_streams->empty());
  // The rest of the stream arrives.
  EXPECT_CALL(*stream_, OnDataAvailable()).WillOnce([&]() {
    // Most classes derived from QuicStream do something like this in
    // OnDataAvailable. This is how FIN-related state is updated.
    std::string buffer;
    stream_->sequencer()->Read(&buffer);
    if (stream_->sequencer()->IsClosed()) {
      stream_->OnFinRead();
    }
  });
  stream_->OnStreamFrame(QuicStreamFrame(
      stream_->id(), false, 0, absl::string_view(data, sizeof(data))));
  EXPECT_TRUE(stream_->read_side_closed());
  ASSERT_EQ(closed_streams->size(), 1);
  EXPECT_EQ((*(closed_streams->begin()))->id(), stream_->id());
}

TEST_P(QuicStreamTest, ReliableSizeAckedAtTimeOfReset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  char data[100];
  memset(data, 0, sizeof(data));
  SendApplicationData(data, 100, false);
  QuicByteCount newly_acked_length = 0;
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
}

TEST_P(QuicStreamTest, BufferedDataInReliableSize) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  EXPECT_CALL(*session_, WritevData(stream_->id(), 100, 0, _, _, _))
      .WillOnce(Return(QuicConsumedData(50, false)));
  char data[100];
  memset(data, 0, sizeof(data));
  // 50 bytes of this will be buffered.
  SendApplicationData(data, 100, false);
  EXPECT_EQ(stream_->BufferedDataBytes(), 50);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  EXPECT_FALSE(stream_->write_side_closed());
  EXPECT_CALL(*session_, WritevData(stream_->id(), 50, 50, _, _, _))
      .WillOnce(Return(QuicConsumedData(50, false)));
  stream_->OnCanWrite();
  // Now that the stream has sent 100 bytes, the write side can be closed.
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState()).Times(1);
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(1);
  QuicByteCount newly_acked_length = 0;
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
}

TEST_P(QuicStreamTest, ReliableSizeIsFinOffset) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  EXPECT_CALL(*session_, WritevData(_, 100, 0, FIN, _, _))
      .WillOnce(Return(QuicConsumedData(100, true)));
  char data[100];
  memset(data, 0, sizeof(data));
  SendApplicationData(data, 100, true);
  // Send STOP_SENDING, but nothing else.
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  EXPECT_CALL(*session_, MaybeSendRstStreamFrame(_, _, _)).Times(0);
  EXPECT_TRUE(stream_->SetReliableSize());
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  // Lose the packet; the stream will not be FINed again.
  stream_->OnStreamFrameLost(0, 100, true);
  EXPECT_CALL(*session_,
              WritevData(stream_->id(), 100, 0, NO_FIN, LOSS_RETRANSMISSION, _))
      .WillOnce(Return(QuicConsumedData(100, true)));
  stream_->OnCanWrite();
}

TEST_P(QuicStreamTest, DataAfterResetStreamAt) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  EXPECT_QUIC_BUG(SendApplicationData(data, 100, false),
                  "Fin already buffered or RESET_STREAM_AT sent");
  EXPECT_EQ(stream_->stream_bytes_written(), 100);
}

TEST_P(QuicStreamTest, SetReliableSizeOnUnidirectionalRead) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      connection_->transport_version(), Perspective::IS_CLIENT);
  TestStream stream(stream_id, session_.get(), READ_UNIDIRECTIONAL);
  EXPECT_FALSE(stream.SetReliableSize());
}

// This covers the case where the read side is already closed, that the zombie
// stream is cleaned up.
TEST_P(QuicStreamTest, ResetStreamAtUnidirectionalWrite) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  const QuicStreamId kId = 3;
  std::unique_ptr<TestStream> stream =
      std::make_unique<TestStream>(kId, session_.get(), WRITE_UNIDIRECTIONAL);
  TestStream* stream_ptr = stream.get();
  session_->ActivateStream(std::move(stream));
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(kId, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(stream_ptr, data, 100, false);
  EXPECT_TRUE(stream_ptr->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_ptr->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  EXPECT_CALL(*stream_ptr, OnWriteSideInDataRecvdState());
  EXPECT_CALL(*connection_, OnStreamReset(kId, _)).Times(1);
  ;
  QuicByteCount newly_acked_length = 0;
  stream_ptr->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                                 QuicTime::Zero(), &newly_acked_length,
                                 /*is_retransmission=*/false);
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  ASSERT_EQ(closed_streams->size(), 1);
  EXPECT_EQ((*(closed_streams->begin()))->id(), kId);
}

// This covers the case where the read side is already closed with FIN, that the
// zombie stream is cleaned up.
TEST_P(QuicStreamTest, ResetStreamAtReadSideFin) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  // Fin the read side.
  QuicStreamId stream_id = stream_->id();
  EXPECT_CALL(*stream_, OnDataAvailable()).Times(1);
  stream_->OnStreamFrame(QuicStreamFrame(stream_->id(), true, 0, ""));
  stream_->OnFinRead();
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  SendApplicationData(data, 100, false);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->PartialResetWriteSide(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
  EXPECT_CALL(*stream_, OnWriteSideInDataRecvdState());
  EXPECT_CALL(*connection_, OnStreamReset(stream_id, _)).Times(1);
  QuicByteCount newly_acked_length = 0;
  stream_->OnStreamFrameAcked(0, 100, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  std::vector<std::unique_ptr<QuicStream>>* closed_streams =
      session_->ClosedStreams();
  ASSERT_EQ(closed_streams->size(), 1);
  EXPECT_EQ((*(closed_streams->begin()))->id(), stream_id);
}

TEST_P(QuicStreamTest, ResetStreamAtAfterStopSending) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  stream_->WriteOrBufferData(absl::string_view(data, 100), false, nullptr);
  EXPECT_TRUE(stream_->SetReliableSize());
  EXPECT_CALL(*session_, MaybeSendResetStreamAtFrame(_, _, _, _)).Times(1);
  stream_->OnStopSending(
      QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED));
}

TEST_P(QuicStreamTest, RejectReliableSizeOldVersion) {
  Initialize();
  if (VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  char data[100];
  memset(data, 0, sizeof(data));
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  stream_->WriteOrBufferData(absl::string_view(data, 100), false, nullptr);
  EXPECT_FALSE(stream_->SetReliableSize());
}

TEST_P(QuicStreamTest, RejectReliableSizeReadOnlyStream) {
  Initialize();
  if (!VersionHasIetfQuicFrames(session_->transport_version())) {
    return;
  }
  auto uni = new StrictMock<TestStream>(6, session_.get(), READ_UNIDIRECTIONAL);
  session_->ActivateStream(absl::WrapUnique(uni));
  EXPECT_FALSE(uni->SetReliableSize());
}

}  // namespace
}  // namespace test
}  // namespace quic
