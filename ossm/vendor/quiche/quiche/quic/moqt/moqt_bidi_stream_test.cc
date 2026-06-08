// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_bidi_stream.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/test_tools/moqt_framer_utils.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

using ::testing::_;
using ::testing::Return;

namespace moqt::test {

class MoqtBidiStreamTest : public quiche::test::QuicheTest {
 public:
  MoqtBidiStreamTest()
      : framer_(true),
        stream_(std::make_unique<MoqtBidiStreamBase>(
            &framer_, deleted_callback_.AsStdFunction(),
            error_callback_.AsStdFunction())) {}

  MoqtFramer framer_;
  testing::MockFunction<void()> deleted_callback_;
  testing::MockFunction<void(MoqtError, absl::string_view)> error_callback_;
  std::unique_ptr<MoqtBidiStreamBase> stream_;
  webtransport::test::MockStream mock_stream_;
};

TEST_F(MoqtBidiStreamTest, AllMessagesRejected) {
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnClientSetupMessage(MoqtClientSetup{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnServerSetupMessage(MoqtServerSetup{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnRequestOkMessage(MoqtRequestOk{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnRequestErrorMessage(MoqtRequestError{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnSubscribeMessage(MoqtSubscribe{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnSubscribeOkMessage(MoqtSubscribeOk{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnUnsubscribeMessage(MoqtUnsubscribe{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishDoneMessage(MoqtPublishDone{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnRequestUpdateMessage(MoqtRequestUpdate{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishNamespaceMessage(MoqtPublishNamespace{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishNamespaceDoneMessage(MoqtPublishNamespaceDone{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishNamespaceCancelMessage(MoqtPublishNamespaceCancel{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnTrackStatusMessage(MoqtTrackStatus{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnGoAwayMessage(MoqtGoAway{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnSubscribeNamespaceMessage(MoqtSubscribeNamespace{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnMaxRequestIdMessage(MoqtMaxRequestId{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnFetchMessage(MoqtFetch{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnFetchCancelMessage(MoqtFetchCancel{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnFetchOkMessage(MoqtFetchOk{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnRequestsBlockedMessage(MoqtRequestsBlocked{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishMessage(MoqtPublish{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnPublishOkMessage(MoqtPublishOk{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
  EXPECT_CALL(error_callback_,
              Call(MoqtError::kProtocolViolation,
                   "Message not allowed for this stream type"));
  stream_->OnObjectAckMessage(MoqtObjectAck{});
  stream_ = std::make_unique<MoqtBidiStreamBase>(
      &framer_, deleted_callback_.AsStdFunction(),
      error_callback_.AsStdFunction());
}

TEST_F(MoqtBidiStreamTest, MessageBufferedThenSent) {
  stream_->set_stream(&mock_stream_);
  EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_stream_, Writev).Times(0);
  stream_->SendRequestOk(0, MessageParameters());
  stream_->SendRequestError(2, RequestErrorCode::kUnauthorized, std::nullopt,
                            "bad request");
  stream_->Fin();
  {
    testing::InSequence seq;
    EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_stream_,
                Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _));
    EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_stream_,
                Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
        .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                     const webtransport::StreamWriteOptions& options) {
          EXPECT_TRUE(options.send_fin());
          return absl::OkStatus();
        });
  }
  stream_->OnCanWrite();
}

TEST_F(MoqtBidiStreamTest, FinSentWhenDrained) {
  stream_->set_stream(&mock_stream_);
  EXPECT_CALL(mock_stream_, Writev)
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  stream_->Fin();
}

TEST_F(MoqtBidiStreamTest, Reset) {
  stream_->set_stream(&mock_stream_);
  EXPECT_CALL(mock_stream_, ResetWithUserCode(1234));
  stream_->Reset(1234);
}

TEST_F(MoqtBidiStreamTest, DeletedCallback) {
  EXPECT_CALL(deleted_callback_, Call());
  stream_.reset();
}

TEST_F(MoqtBidiStreamTest, PendingQueueFull) {
  stream_->set_stream(&mock_stream_);
  EXPECT_CALL(mock_stream_, CanWrite).WillRepeatedly(Return(false));
  for (int i = 0; i < 100; ++i) {  // kMaxPendingMessages = 100.
    EXPECT_FALSE(stream_->QueueIsFull());
    stream_->SendOrBufferMessage(
        framer_.SerializeRequestUpdate(MoqtRequestUpdate{}));
  }
  EXPECT_TRUE(stream_->QueueIsFull());
  EXPECT_CALL(error_callback_, Call(MoqtError::kInternalError, _));
  stream_->SendOrBufferMessage(
      framer_.SerializeRequestUpdate(MoqtRequestUpdate{}));
  EXPECT_TRUE(stream_->QueueIsFull());
}

}  // namespace moqt::test
