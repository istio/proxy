// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_datagram_queue.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_mem_slice.h"
#include "quiche/common/platform/api/quiche_reference_counted.h"
#include "quiche/common/quiche_buffer_allocator.h"

namespace quic {
namespace test {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Return;

class EstablishedCryptoStream : public MockQuicCryptoStream {
 public:
  using MockQuicCryptoStream::MockQuicCryptoStream;

  bool encryption_established() const override { return true; }
};

class QuicDatagramQueueObserver final : public QuicDatagramQueue::Observer {
 public:
  class Context : public quiche::QuicheReferenceCounted {
   public:
    std::vector<std::optional<MessageStatus>> statuses;
  };

  QuicDatagramQueueObserver() : context_(new Context()) {}
  QuicDatagramQueueObserver(const QuicDatagramQueueObserver&) = delete;
  QuicDatagramQueueObserver& operator=(const QuicDatagramQueueObserver&) =
      delete;

  void OnDatagramProcessed(std::optional<MessageStatus> status) override {
    context_->statuses.push_back(std::move(status));
  }

  const quiche::QuicheReferenceCountedPointer<Context>& context() {
    return context_;
  }

 private:
  quiche::QuicheReferenceCountedPointer<Context> context_;
};

class QuicDatagramQueueTestBase : public QuicTest {
 protected:
  QuicDatagramQueueTestBase()
      : connection_(new MockQuicConnection(&helper_, &alarm_factory_,
                                           Perspective::IS_CLIENT)),
        session_(connection_) {
    session_.SetCryptoStream(new EstablishedCryptoStream(&session_));
    connection_->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<NullEncrypter>(connection_->perspective()));
  }

  ~QuicDatagramQueueTestBase() = default;

  quiche::QuicheMemSlice CreateMemSlice(absl::string_view data) {
    return quiche::QuicheMemSlice(quiche::QuicheBuffer::Copy(
        helper_.GetStreamSendBufferAllocator(), data));
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;  // Owned by |session_|.
  MockQuicSession session_;
};

class QuicDatagramQueueTest : public QuicDatagramQueueTestBase {
 public:
  QuicDatagramQueueTest() : queue_(&session_) {}

 protected:
  QuicDatagramQueue queue_;
};

TEST_F(QuicDatagramQueueTest, SendDatagramImmediately) {
  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_SUCCESS));
  MessageStatus status = queue_.SendOrQueueDatagram(CreateMemSlice("test"));
  EXPECT_EQ(MESSAGE_STATUS_SUCCESS, status);
  EXPECT_EQ(0u, queue_.queue_size());
}

TEST_F(QuicDatagramQueueTest, SendDatagramAfterBuffering) {
  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));
  MessageStatus initial_status =
      queue_.SendOrQueueDatagram(CreateMemSlice("test"));
  EXPECT_EQ(MESSAGE_STATUS_BLOCKED, initial_status);
  EXPECT_EQ(1u, queue_.queue_size());

  // Verify getting write blocked does not remove the datagram from the queue.
  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));
  std::optional<MessageStatus> status = queue_.TrySendingNextDatagram();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(MESSAGE_STATUS_BLOCKED, *status);
  EXPECT_EQ(1u, queue_.queue_size());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_SUCCESS));
  status = queue_.TrySendingNextDatagram();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(MESSAGE_STATUS_SUCCESS, *status);
  EXPECT_EQ(0u, queue_.queue_size());
}

TEST_F(QuicDatagramQueueTest, EmptyBuffer) {
  std::optional<MessageStatus> status = queue_.TrySendingNextDatagram();
  EXPECT_FALSE(status.has_value());

  size_t num_messages = queue_.SendDatagrams();
  EXPECT_EQ(0u, num_messages);
}

TEST_F(QuicDatagramQueueTest, MultipleDatagrams) {
  // Note that SendMessage() is called only once here, since all the remaining
  // messages are automatically queued due to the queue being non-empty.
  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));
  queue_.SendOrQueueDatagram(CreateMemSlice("a"));
  queue_.SendOrQueueDatagram(CreateMemSlice("b"));
  queue_.SendOrQueueDatagram(CreateMemSlice("c"));
  queue_.SendOrQueueDatagram(CreateMemSlice("d"));
  queue_.SendOrQueueDatagram(CreateMemSlice("e"));

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .Times(5)
      .WillRepeatedly(Return(MESSAGE_STATUS_SUCCESS));
  size_t num_messages = queue_.SendDatagrams();
  EXPECT_EQ(5u, num_messages);
}

TEST_F(QuicDatagramQueueTest, DefaultMaxTimeInQueue) {
  EXPECT_EQ(QuicTime::Delta::Zero(),
            connection_->sent_packet_manager().GetRttStats()->min_rtt());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(4), queue_.GetMaxTimeInQueue());

  RttStats* stats =
      const_cast<RttStats*>(connection_->sent_packet_manager().GetRttStats());
  stats->UpdateRtt(QuicTime::Delta::FromMilliseconds(100),
                   QuicTime::Delta::Zero(), helper_.GetClock()->Now());
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(125), queue_.GetMaxTimeInQueue());
}

TEST_F(QuicDatagramQueueTest, Expiry) {
  constexpr QuicTime::Delta expiry = QuicTime::Delta::FromMilliseconds(100);
  queue_.SetMaxTimeInQueue(expiry);

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));
  queue_.SendOrQueueDatagram(CreateMemSlice("a"));
  helper_.AdvanceTime(0.6 * expiry);
  queue_.SendOrQueueDatagram(CreateMemSlice("b"));
  helper_.AdvanceTime(0.6 * expiry);
  queue_.SendOrQueueDatagram(CreateMemSlice("c"));

  std::vector<std::string> messages;
  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillRepeatedly([&messages](QuicMessageId /*id*/,
                                  absl::Span<quiche::QuicheMemSlice> message,
                                  bool /*flush*/) {
        messages.push_back(std::string(message[0].AsStringView()));
        return MESSAGE_STATUS_SUCCESS;
      });
  EXPECT_EQ(2u, queue_.SendDatagrams());
  EXPECT_THAT(messages, ElementsAre("b", "c"));
}

TEST_F(QuicDatagramQueueTest, ExpireAll) {
  constexpr QuicTime::Delta expiry = QuicTime::Delta::FromMilliseconds(100);
  queue_.SetMaxTimeInQueue(expiry);

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));
  queue_.SendOrQueueDatagram(CreateMemSlice("a"));
  queue_.SendOrQueueDatagram(CreateMemSlice("b"));
  queue_.SendOrQueueDatagram(CreateMemSlice("c"));

  helper_.AdvanceTime(100 * expiry);
  EXPECT_CALL(*connection_, SendMessage(_, _, _)).Times(0);
  EXPECT_EQ(0u, queue_.SendDatagrams());
}

class QuicDatagramQueueWithObserverTest : public QuicDatagramQueueTestBase {
 public:
  QuicDatagramQueueWithObserverTest()
      : observer_(std::make_unique<QuicDatagramQueueObserver>()),
        context_(observer_->context()),
        queue_(&session_, std::move(observer_)) {}

 protected:
  // This is moved out immediately.
  std::unique_ptr<QuicDatagramQueueObserver> observer_;

  quiche::QuicheReferenceCountedPointer<QuicDatagramQueueObserver::Context>
      context_;
  QuicDatagramQueue queue_;
};

TEST_F(QuicDatagramQueueWithObserverTest, ObserveSuccessImmediately) {
  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_SUCCESS));

  EXPECT_EQ(MESSAGE_STATUS_SUCCESS,
            queue_.SendOrQueueDatagram(CreateMemSlice("a")));

  EXPECT_THAT(context_->statuses, ElementsAre(MESSAGE_STATUS_SUCCESS));
}

TEST_F(QuicDatagramQueueWithObserverTest, ObserveFailureImmediately) {
  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_TOO_LARGE));

  EXPECT_EQ(MESSAGE_STATUS_TOO_LARGE,
            queue_.SendOrQueueDatagram(CreateMemSlice("a")));

  EXPECT_THAT(context_->statuses, ElementsAre(MESSAGE_STATUS_TOO_LARGE));
}

TEST_F(QuicDatagramQueueWithObserverTest, BlockingShouldNotBeObserved) {
  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillRepeatedly(Return(MESSAGE_STATUS_BLOCKED));

  EXPECT_EQ(MESSAGE_STATUS_BLOCKED,
            queue_.SendOrQueueDatagram(CreateMemSlice("a")));
  EXPECT_EQ(0u, queue_.SendDatagrams());

  EXPECT_TRUE(context_->statuses.empty());
}

TEST_F(QuicDatagramQueueWithObserverTest, ObserveSuccessAfterBuffering) {
  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));

  EXPECT_EQ(MESSAGE_STATUS_BLOCKED,
            queue_.SendOrQueueDatagram(CreateMemSlice("a")));

  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_SUCCESS));

  EXPECT_EQ(1u, queue_.SendDatagrams());
  EXPECT_THAT(context_->statuses, ElementsAre(MESSAGE_STATUS_SUCCESS));
}

TEST_F(QuicDatagramQueueWithObserverTest, ObserveExpiry) {
  constexpr QuicTime::Delta expiry = QuicTime::Delta::FromMilliseconds(100);
  queue_.SetMaxTimeInQueue(expiry);

  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _))
      .WillOnce(Return(MESSAGE_STATUS_BLOCKED));

  EXPECT_EQ(MESSAGE_STATUS_BLOCKED,
            queue_.SendOrQueueDatagram(CreateMemSlice("a")));

  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_CALL(*connection_, SendMessage(_, _, _)).Times(0);
  helper_.AdvanceTime(100 * expiry);

  EXPECT_TRUE(context_->statuses.empty());

  EXPECT_EQ(0u, queue_.SendDatagrams());
  EXPECT_THAT(context_->statuses, ElementsAre(std::nullopt));
}

}  // namespace
}  // namespace test
}  // namespace quic
