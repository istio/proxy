// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A universal test for all event loops supported by the build of QUICHE in
// question.
//
// This test is very similar to QuicPollEventLoopTest, however, there are some
// notable differences:
//   (1) This test uses the real clock, since the event loop implementation may
//       not support accepting a mock clock.
//   (2) This test covers both level-triggered and edge-triggered event loops.

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/io/quic_default_event_loop.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic::test {
namespace {

using testing::_;
using testing::AtMost;

MATCHER_P(HasFlagSet, value, "Checks a flag in a bit mask") {
  return (arg & value) != 0;
}

constexpr QuicSocketEventMask kAllEvents =
    kSocketEventReadable | kSocketEventWritable | kSocketEventError;

class MockQuicSocketEventListener : public QuicSocketEventListener {
 public:
  MOCK_METHOD(void, OnSocketEvent,
              (QuicEventLoop* /*event_loop*/, SocketFd /*fd*/,
               QuicSocketEventMask /*events*/),
              (override));
};

class MockDelegate : public QuicAlarm::Delegate {
 public:
  QuicConnectionContext* GetConnectionContext() override { return nullptr; }
  MOCK_METHOD(void, OnAlarm, (), (override));
};

void SetNonBlocking(int fd) {
  QUICHE_CHECK(::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK) == 0)
      << "Failed to mark FD non-blocking, errno: " << errno;
}

class QuicEventLoopFactoryTest
    : public QuicTestWithParam<QuicEventLoopFactory*> {
 public:
  void SetUp() override {
    loop_ = GetParam()->Create(&clock_);
    factory_ = loop_->CreateAlarmFactory();
    int fds[2];
    int result = ::pipe(fds);
    QUICHE_CHECK(result >= 0) << "Failed to create a pipe, errno: " << errno;
    read_fd_ = fds[0];
    write_fd_ = fds[1];

    SetNonBlocking(read_fd_);
    SetNonBlocking(write_fd_);
  }

  void TearDown() override {
    factory_.reset();
    loop_.reset();
    // Epoll-based event loop automatically removes registered FDs from the
    // Epoll set, which should happen before these FDs are closed.
    close(read_fd_);
    close(write_fd_);
  }

  std::pair<std::unique_ptr<QuicAlarm>, MockDelegate*> CreateAlarm() {
    auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
    MockDelegate* delegate_unowned = delegate.get();
    auto alarm = absl::WrapUnique(factory_->CreateAlarm(delegate.release()));
    return std::make_pair(std::move(alarm), delegate_unowned);
  }

  template <typename Condition>
  void RunEventLoopUntil(Condition condition, QuicTime::Delta timeout) {
    const QuicTime end = clock_.Now() + timeout;
    while (!condition() && clock_.Now() < end) {
      loop_->RunEventLoopOnce(end - clock_.Now());
    }
  }

 protected:
  QuicDefaultClock clock_;
  std::unique_ptr<QuicEventLoop> loop_;
  std::unique_ptr<QuicAlarmFactory> factory_;
  int read_fd_;
  int write_fd_;
};

std::string GetTestParamName(
    ::testing::TestParamInfo<QuicEventLoopFactory*> info) {
  return EscapeTestParamName(info.param->GetName());
}

INSTANTIATE_TEST_SUITE_P(QuicEventLoopFactoryTests, QuicEventLoopFactoryTest,
                         ::testing::ValuesIn(GetAllSupportedEventLoops()),
                         GetTestParamName);

TEST_P(QuicEventLoopFactoryTest, NothingHappens) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kAllEvents, &listener));
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  // Attempt double-registration.
  EXPECT_FALSE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(4));
  // Expect no further calls.
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(5));
}

TEST_P(QuicEventLoopFactoryTest, RearmWriter) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  if (loop_->SupportsEdgeTriggered()) {
    EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable))
        .Times(1);
    loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
    loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  } else {
    EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable))
        .Times(2);
    loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
    ASSERT_TRUE(loop_->RearmSocket(write_fd_, kSocketEventWritable));
    loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  }
}

TEST_P(QuicEventLoopFactoryTest, Readable) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kAllEvents, &listener));

  ASSERT_EQ(4, write(write_fd_, "test", 4));
  EXPECT_CALL(listener, OnSocketEvent(_, read_fd_, kSocketEventReadable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  // Expect no further calls.
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
}

// A common pattern: read a limited amount of data from an FD, and expect to
// read the remainder on the next operation.
TEST_P(QuicEventLoopFactoryTest, ArtificialNotifyFromCallback) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kSocketEventReadable, &listener));

  constexpr absl::string_view kData = "test test test test test test test ";
  constexpr size_t kTimes = kData.size() / 5;
  ASSERT_EQ(kData.size(), write(write_fd_, kData.data(), kData.size()));
  EXPECT_CALL(listener, OnSocketEvent(_, read_fd_, kSocketEventReadable))
      .Times(loop_->SupportsEdgeTriggered() ? (kTimes + 1) : kTimes)
      .WillRepeatedly([&]() {
        char buf[5];
        int read_result = read(read_fd_, buf, sizeof(buf));
        if (read_result > 0) {
          ASSERT_EQ(read_result, 5);
          if (loop_->SupportsEdgeTriggered()) {
            EXPECT_TRUE(
                loop_->ArtificiallyNotifyEvent(read_fd_, kSocketEventReadable));
          } else {
            EXPECT_TRUE(loop_->RearmSocket(read_fd_, kSocketEventReadable));
          }
        } else {
          EXPECT_EQ(errno, EAGAIN);
        }
      });
  for (size_t i = 0; i < kTimes + 2; i++) {
    loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  }
}

// Verify that artificial events are notified on the next iteration. This is to
// prevent infinite loops in RunEventLoopOnce when the event callback keeps
// adding artificial events.
TEST_P(QuicEventLoopFactoryTest, ArtificialNotifyOncePerIteration) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kSocketEventReadable, &listener));

  constexpr absl::string_view kData = "test test test test test test test ";
  ASSERT_EQ(kData.size(), write(write_fd_, kData.data(), kData.size()));

  int64_t read_event_count_ = 0;
  EXPECT_CALL(listener, OnSocketEvent(_, read_fd_, kSocketEventReadable))
      .WillRepeatedly([&]() {
        read_event_count_++;
        EXPECT_TRUE(
            loop_->ArtificiallyNotifyEvent(read_fd_, kSocketEventReadable));
      });
  for (size_t i = 1; i < 5; i++) {
    loop_->RunEventLoopOnce(QuicTime::Delta::FromSeconds(10));
    EXPECT_EQ(read_event_count_, i);
  }
}

TEST_P(QuicEventLoopFactoryTest, WriterUnblocked) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));

  int io_result;
  std::string data(2048, 'a');
  do {
    io_result = write(write_fd_, data.data(), data.size());
  } while (io_result > 0);
  ASSERT_EQ(errno, EAGAIN);

  // Rearm if necessary and expect no immediate calls.
  if (!loop_->SupportsEdgeTriggered()) {
    ASSERT_TRUE(loop_->RearmSocket(write_fd_, kSocketEventWritable));
  }
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));

  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable));
  do {
    io_result = read(read_fd_, data.data(), data.size());
  } while (io_result > 0);
  ASSERT_EQ(errno, EAGAIN);
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
}

TEST_P(QuicEventLoopFactoryTest, ArtificialEvent) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kAllEvents, &listener));
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  ASSERT_TRUE(loop_->ArtificiallyNotifyEvent(read_fd_, kSocketEventReadable));

  EXPECT_CALL(listener, OnSocketEvent(_, read_fd_, kSocketEventReadable));
  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
}

TEST_P(QuicEventLoopFactoryTest, Unregister) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));
  ASSERT_TRUE(loop_->UnregisterSocket(write_fd_));

  // Expect nothing to happen.
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));

  EXPECT_FALSE(loop_->UnregisterSocket(write_fd_));
  if (!loop_->SupportsEdgeTriggered()) {
    EXPECT_FALSE(loop_->RearmSocket(write_fd_, kSocketEventWritable));
  }
  EXPECT_FALSE(loop_->ArtificiallyNotifyEvent(write_fd_, kSocketEventWritable));
}

TEST_P(QuicEventLoopFactoryTest, UnregisterInsideEventHandler) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(read_fd_, kAllEvents, &listener));
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  // We are not guaranteed the order in which those events will happen, so we
  // try to accommodate both possibilities.
  int total_called = 0;
  EXPECT_CALL(listener, OnSocketEvent(_, read_fd_, kSocketEventReadable))
      .Times(AtMost(1))
      .WillOnce([&]() {
        ++total_called;
        ASSERT_TRUE(loop_->UnregisterSocket(write_fd_));
      });
  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable))
      .Times(AtMost(1))
      .WillOnce([&]() {
        ++total_called;
        ASSERT_TRUE(loop_->UnregisterSocket(read_fd_));
      });
  ASSERT_TRUE(loop_->ArtificiallyNotifyEvent(read_fd_, kSocketEventReadable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
  EXPECT_EQ(total_called, 1);
}

TEST_P(QuicEventLoopFactoryTest, UnregisterSelfInsideEventHandler) {
  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(write_fd_, kAllEvents, &listener));

  EXPECT_CALL(listener, OnSocketEvent(_, write_fd_, kSocketEventWritable))
      .WillOnce([&]() { ASSERT_TRUE(loop_->UnregisterSocket(write_fd_)); });
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(1));
}

// Creates a bidirectional socket and tests its behavior when it's both readable
// and writable.
TEST_P(QuicEventLoopFactoryTest, ReadWriteSocket) {
  int sockets[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
  SetNonBlocking(sockets[0]);
  SetNonBlocking(sockets[1]);

  testing::StrictMock<MockQuicSocketEventListener> listener;
  ASSERT_TRUE(loop_->RegisterSocket(sockets[0], kAllEvents, &listener));
  EXPECT_CALL(listener, OnSocketEvent(_, sockets[0], kSocketEventWritable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(4));

  int io_result;
  std::string data(2048, 'a');
  do {
    io_result = write(sockets[0], data.data(), data.size());
  } while (io_result > 0);
  ASSERT_EQ(errno, EAGAIN);

  if (!loop_->SupportsEdgeTriggered()) {
    ASSERT_TRUE(loop_->RearmSocket(sockets[0], kSocketEventWritable));
  }
  // We are not write-blocked, so this should not notify.
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(4));

  EXPECT_GT(write(sockets[1], data.data(), data.size()), 0);
  EXPECT_CALL(listener, OnSocketEvent(_, sockets[0], kSocketEventReadable));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(4));

  do {
    char buffer[2048];
    io_result = read(sockets[1], buffer, sizeof(buffer));
  } while (io_result > 0);
  ASSERT_EQ(errno, EAGAIN);
  // Here, we can receive either "writable" or "readable and writable"
  // notification depending on the backend in question.
  EXPECT_CALL(listener,
              OnSocketEvent(_, sockets[0], HasFlagSet(kSocketEventWritable)));
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(4));

  EXPECT_TRUE(loop_->UnregisterSocket(sockets[0]));
  close(sockets[0]);
  close(sockets[1]);
}

TEST_P(QuicEventLoopFactoryTest, AlarmInFuture) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm, delegate] = CreateAlarm();

  alarm->Set(clock_.Now() + kAlarmTimeout);

  bool alarm_called = false;
  EXPECT_CALL(*delegate, OnAlarm()).WillOnce([&]() { alarm_called = true; });
  RunEventLoopUntil([&]() { return alarm_called; },
                    QuicTime::Delta::FromMilliseconds(100));
}

TEST_P(QuicEventLoopFactoryTest, AlarmsInPast) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm1, delegate1] = CreateAlarm();
  auto [alarm2, delegate2] = CreateAlarm();

  alarm1->Set(clock_.Now() - 2 * kAlarmTimeout);
  alarm2->Set(clock_.Now() - kAlarmTimeout);

  {
    testing::InSequence s;
    EXPECT_CALL(*delegate1, OnAlarm());
    EXPECT_CALL(*delegate2, OnAlarm());
  }
  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(100));
}

TEST_P(QuicEventLoopFactoryTest, AlarmCancelled) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm, delegate] = CreateAlarm();

  alarm->Set(clock_.Now() + kAlarmTimeout);
  alarm->Cancel();

  loop_->RunEventLoopOnce(kAlarmTimeout * 2);
}

TEST_P(QuicEventLoopFactoryTest, AlarmCancelledAndSetAgain) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm, delegate] = CreateAlarm();

  alarm->Set(clock_.Now() + kAlarmTimeout);
  alarm->Cancel();
  alarm->Set(clock_.Now() + 2 * kAlarmTimeout);

  bool alarm_called = false;
  EXPECT_CALL(*delegate, OnAlarm()).WillOnce([&]() { alarm_called = true; });
  RunEventLoopUntil([&]() { return alarm_called; },
                    QuicTime::Delta::FromMilliseconds(100));
}

TEST_P(QuicEventLoopFactoryTest, AlarmCancelsAnotherAlarm) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm1_ptr, delegate1] = CreateAlarm();
  auto [alarm2_ptr, delegate2] = CreateAlarm();

  QuicAlarm& alarm1 = *alarm1_ptr;
  QuicAlarm& alarm2 = *alarm2_ptr;
  alarm1.Set(clock_.Now() - kAlarmTimeout);
  alarm2.Set(clock_.Now() - kAlarmTimeout);

  int alarms_called = 0;
  // Since the order in which alarms are cancelled is not well-determined, make
  // each one cancel another.
  EXPECT_CALL(*delegate1, OnAlarm()).Times(AtMost(1)).WillOnce([&]() {
    alarm2.Cancel();
    ++alarms_called;
  });
  EXPECT_CALL(*delegate2, OnAlarm()).Times(AtMost(1)).WillOnce([&]() {
    alarm1.Cancel();
    ++alarms_called;
  });
  // Run event loop twice to ensure the second alarm is not called after two
  // iterations.
  loop_->RunEventLoopOnce(kAlarmTimeout * 2);
  loop_->RunEventLoopOnce(kAlarmTimeout * 2);
  EXPECT_EQ(alarms_called, 1);
}

TEST_P(QuicEventLoopFactoryTest, DestructorWithPendingAlarm) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(5);
  auto [alarm1_ptr, delegate1] = CreateAlarm();

  alarm1_ptr->Set(clock_.Now() + kAlarmTimeout);
  // Expect destructor to cleanly unregister itself before the event loop is
  // gone.
}

TEST_P(QuicEventLoopFactoryTest, NegativeTimeout) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromSeconds(300);
  auto [alarm1_ptr, delegate1] = CreateAlarm();

  alarm1_ptr->Set(clock_.Now() + kAlarmTimeout);

  loop_->RunEventLoopOnce(QuicTime::Delta::FromMilliseconds(-1));
}

TEST_P(QuicEventLoopFactoryTest, ScheduleAlarmInPastFromInsideAlarm) {
  constexpr auto kAlarmTimeout = QuicTime::Delta::FromMilliseconds(20);
  auto [alarm1_ptr, delegate1] = CreateAlarm();
  auto [alarm2_ptr, delegate2] = CreateAlarm();

  alarm1_ptr->Set(clock_.Now() - kAlarmTimeout);
  EXPECT_CALL(*delegate1, OnAlarm())
      .WillOnce([&, alarm2_unowned = alarm2_ptr.get()]() {
        alarm2_unowned->Set(clock_.Now() - 2 * kAlarmTimeout);
      });
  bool fired = false;
  EXPECT_CALL(*delegate2, OnAlarm()).WillOnce([&]() { fired = true; });

  RunEventLoopUntil([&]() { return fired; },
                    QuicTime::Delta::FromMilliseconds(100));
}

}  // namespace
}  // namespace quic::test
