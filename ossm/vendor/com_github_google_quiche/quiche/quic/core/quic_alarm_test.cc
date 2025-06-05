// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_alarm.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "quiche/quic/core/quic_connection_context.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"

using testing::ElementsAre;
using testing::Invoke;
using testing::Return;

namespace quic {
namespace test {
namespace {

class TraceCollector : public QuicConnectionTracer {
 public:
  ~TraceCollector() override = default;

  void PrintLiteral(const char* literal) override { trace_.push_back(literal); }

  void PrintString(absl::string_view s) override {
    trace_.push_back(std::string(s));
  }

  const std::vector<std::string>& trace() const { return trace_; }

 private:
  std::vector<std::string> trace_;
};

class MockDelegate : public QuicAlarm::Delegate {
 public:
  MOCK_METHOD(QuicConnectionContext*, GetConnectionContext, (), (override));
  MOCK_METHOD(void, OnAlarm, (), (override));
};

class DestructiveDelegate : public QuicAlarm::DelegateWithoutContext {
 public:
  DestructiveDelegate() : alarm_(nullptr) {}

  void set_alarm(QuicAlarm* alarm) { alarm_ = alarm; }

  void OnAlarm() override {
    QUICHE_DCHECK(alarm_);
    delete alarm_;
  }

 private:
  QuicAlarm* alarm_;
};

class TestAlarm : public QuicAlarm {
 public:
  explicit TestAlarm(QuicAlarm::Delegate* delegate)
      : QuicAlarm(QuicArenaScopedPtr<QuicAlarm::Delegate>(delegate)) {}

  bool scheduled() const { return scheduled_; }

  void FireAlarm() {
    scheduled_ = false;
    Fire();
  }

 protected:
  void SetImpl() override {
    QUICHE_DCHECK(deadline().IsInitialized());
    scheduled_ = true;
  }

  void CancelImpl() override {
    QUICHE_DCHECK(!deadline().IsInitialized());
    scheduled_ = false;
  }

 private:
  bool scheduled_;
};

class DestructiveAlarm : public QuicAlarm {
 public:
  explicit DestructiveAlarm(DestructiveDelegate* delegate)
      : QuicAlarm(QuicArenaScopedPtr<DestructiveDelegate>(delegate)) {}

  void FireAlarm() { Fire(); }

 protected:
  void SetImpl() override {}

  void CancelImpl() override {}
};

class QuicAlarmTest : public QuicTest {
 public:
  QuicAlarmTest()
      : delegate_(new MockDelegate()),
        alarm_(delegate_),
        deadline_(QuicTime::Zero() + QuicTime::Delta::FromSeconds(7)),
        deadline2_(QuicTime::Zero() + QuicTime::Delta::FromSeconds(14)),
        new_deadline_(QuicTime::Zero()) {}

  void ResetAlarm() { alarm_.Set(new_deadline_); }

  MockDelegate* delegate_;  // not owned
  TestAlarm alarm_;
  QuicTime deadline_;
  QuicTime deadline2_;
  QuicTime new_deadline_;
};

TEST_F(QuicAlarmTest, IsSet) { EXPECT_FALSE(alarm_.IsSet()); }

TEST_F(QuicAlarmTest, Set) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(deadline, alarm_.deadline());
}

TEST_F(QuicAlarmTest, Cancel) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  alarm_.Cancel();
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, PermanentCancel) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  alarm_.PermanentCancel();
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());

  EXPECT_QUIC_BUG(alarm_.Set(deadline),
                  "Set called after alarm is permanently cancelled");
  EXPECT_TRUE(alarm_.IsPermanentlyCancelled());
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());

  EXPECT_QUIC_BUG(alarm_.Update(deadline, QuicTime::Delta::Zero()),
                  "Update called after alarm is permanently cancelled");
  EXPECT_TRUE(alarm_.IsPermanentlyCancelled());
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, Update) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  QuicTime new_deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(8);
  alarm_.Update(new_deadline, QuicTime::Delta::Zero());
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(new_deadline, alarm_.deadline());
}

TEST_F(QuicAlarmTest, UpdateWithZero) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  alarm_.Update(QuicTime::Zero(), QuicTime::Delta::Zero());
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, Fire) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);
  EXPECT_CALL(*delegate_, OnAlarm());
  alarm_.FireAlarm();
  EXPECT_FALSE(alarm_.IsSet());
  EXPECT_FALSE(alarm_.scheduled());
  EXPECT_EQ(QuicTime::Zero(), alarm_.deadline());
}

TEST_F(QuicAlarmTest, FireAndResetViaSet) {
  alarm_.Set(deadline_);
  new_deadline_ = deadline2_;
  EXPECT_CALL(*delegate_, OnAlarm())
      .WillOnce(Invoke(this, &QuicAlarmTest::ResetAlarm));
  alarm_.FireAlarm();
  EXPECT_TRUE(alarm_.IsSet());
  EXPECT_TRUE(alarm_.scheduled());
  EXPECT_EQ(deadline2_, alarm_.deadline());
}

TEST_F(QuicAlarmTest, FireDestroysAlarm) {
  DestructiveDelegate* delegate(new DestructiveDelegate);
  DestructiveAlarm* alarm = new DestructiveAlarm(delegate);
  delegate->set_alarm(alarm);
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm->Set(deadline);
  // This should not crash, even though it will destroy alarm.
  alarm->FireAlarm();
}

TEST_F(QuicAlarmTest, NullAlarmContext) {
  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);

  EXPECT_CALL(*delegate_, GetConnectionContext()).WillOnce(Return(nullptr));

  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(Invoke([] {
    QUIC_TRACELITERAL("Alarm fired.");
  }));
  alarm_.FireAlarm();
}

TEST_F(QuicAlarmTest, AlarmContextWithNullTracer) {
  QuicConnectionContext context;
  ASSERT_EQ(context.tracer, nullptr);

  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);

  EXPECT_CALL(*delegate_, GetConnectionContext()).WillOnce(Return(&context));

  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(Invoke([] {
    QUIC_TRACELITERAL("Alarm fired.");
  }));
  alarm_.FireAlarm();
}

TEST_F(QuicAlarmTest, AlarmContextWithTracer) {
  QuicConnectionContext context;
  std::unique_ptr<TraceCollector> tracer = std::make_unique<TraceCollector>();
  const TraceCollector& tracer_ref = *tracer;
  context.tracer = std::move(tracer);

  QuicTime deadline = QuicTime::Zero() + QuicTime::Delta::FromSeconds(7);
  alarm_.Set(deadline);

  EXPECT_CALL(*delegate_, GetConnectionContext()).WillOnce(Return(&context));

  EXPECT_CALL(*delegate_, OnAlarm()).WillOnce(Invoke([] {
    QUIC_TRACELITERAL("Alarm fired.");
  }));

  // Since |context| is not installed in the current thread, the messages before
  // and after FireAlarm() should not be collected by |tracer|.
  QUIC_TRACELITERAL("Should not be collected before alarm.");
  alarm_.FireAlarm();
  QUIC_TRACELITERAL("Should not be collected after alarm.");

  EXPECT_THAT(tracer_ref.trace(), ElementsAre("Alarm fired."));
}

}  // namespace
}  // namespace test
}  // namespace quic
