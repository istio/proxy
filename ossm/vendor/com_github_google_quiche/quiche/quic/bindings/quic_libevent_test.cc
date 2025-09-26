// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/bindings/quic_libevent.h"

#include <atomic>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_thread.h"

namespace quic::test {
namespace {

class FailureAlarmDelegate : public QuicAlarm::Delegate {
 public:
  QuicConnectionContext* GetConnectionContext() override { return nullptr; }
  void OnAlarm() override { ADD_FAILURE() << "Test timed out"; }
};

class LoopBreakThread : public QuicThread {
 public:
  LoopBreakThread(LibeventQuicEventLoop* loop)
      : QuicThread("LoopBreakThread"), loop_(loop) {}

  void Run() override {
    // Make sure the other thread has actually made the blocking poll/epoll/etc
    // call before calling WakeUp().
    absl::SleepFor(absl::Milliseconds(250));

    loop_broken_.store(true);
    loop_->WakeUp();
  }

  std::atomic<int>& loop_broken() { return loop_broken_; }

 private:
  LibeventQuicEventLoop* loop_;
  std::atomic<int> loop_broken_ = 0;
};

TEST(QuicLibeventTest, WakeUpFromAnotherThread) {
  QuicClock* clock = QuicDefaultClock::Get();
  auto event_loop_owned = QuicLibeventEventLoopFactory::Get()->Create(clock);
  LibeventQuicEventLoop* event_loop =
      static_cast<LibeventQuicEventLoop*>(event_loop_owned.get());
  std::unique_ptr<QuicAlarmFactory> alarm_factory =
      event_loop->CreateAlarmFactory();
  std::unique_ptr<QuicAlarm> timeout_alarm =
      absl::WrapUnique(alarm_factory->CreateAlarm(new FailureAlarmDelegate()));

  const QuicTime kTimeoutAt = clock->Now() + QuicTime::Delta::FromSeconds(10);
  timeout_alarm->Set(kTimeoutAt);

  LoopBreakThread thread(event_loop);
  thread.Start();
  event_loop->RunEventLoopOnce(QuicTime::Delta::FromSeconds(5 * 60));
  EXPECT_TRUE(thread.loop_broken().load());
  thread.Join();
}

}  // namespace
}  // namespace quic::test
