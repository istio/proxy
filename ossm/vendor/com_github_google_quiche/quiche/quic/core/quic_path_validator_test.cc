// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_path_validator.h"

#include <memory>

#include "quiche/quic/core/frames/quic_path_challenge_frame.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/mock_random.h"
#include "quiche/quic/test_tools/quic_path_validator_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace quic {
namespace test {

class MockSendDelegate : public QuicPathValidator::SendDelegate {
 public:
  // Send a PATH_CHALLENGE frame using given path information and populate
  // |data_buffer| with the frame payload. Return true if the validator should
  // move forward in validation, i.e. arm the retry timer.
  MOCK_METHOD(bool, SendPathChallenge,
              (const QuicPathFrameBuffer&, const QuicSocketAddress&,
               const QuicSocketAddress&, const QuicSocketAddress&,
               QuicPacketWriter*),
              (override));

  MOCK_METHOD(QuicTime, GetRetryTimeout,
              (const QuicSocketAddress&, QuicPacketWriter*), (const, override));
};

class QuicPathValidatorTest : public QuicTest {
 public:
  QuicPathValidatorTest()
      : path_validator_(&alarm_factory_, &arena_, &send_delegate_, &random_,
                        &clock_,
                        /*context=*/nullptr),
        context_(new MockQuicPathValidationContext(
            self_address_, peer_address_, effective_peer_address_, &writer_)),
        result_delegate_(
            new testing::StrictMock<MockQuicPathValidationResultDelegate>()) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    ON_CALL(send_delegate_, GetRetryTimeout(_, _))
        .WillByDefault(
            Return(clock_.ApproximateNow() +
                   3 * QuicTime::Delta::FromMilliseconds(kInitialRttMs)));
  }

 protected:
  quic::test::MockAlarmFactory alarm_factory_;
  MockSendDelegate send_delegate_;
  MockRandom random_;
  MockClock clock_;
  QuicConnectionArena arena_;
  QuicPathValidator path_validator_;
  QuicSocketAddress self_address_{QuicIpAddress::Any4(), 443};
  QuicSocketAddress peer_address_{QuicIpAddress::Loopback4(), 443};
  QuicSocketAddress effective_peer_address_{QuicIpAddress::Loopback4(), 12345};
  MockPacketWriter writer_;
  MockQuicPathValidationContext* context_;
  MockQuicPathValidationResultDelegate* result_delegate_;
};

TEST_F(QuicPathValidatorTest, PathValidationSuccessOnFirstRound) {
  QuicPathFrameBuffer challenge_data;
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer& payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        memcpy(challenge_data.data(), payload.data(), payload.size());
        return true;
      }));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_));
  const QuicTime expected_start_time = clock_.Now();
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);
  EXPECT_TRUE(path_validator_.HasPendingPathValidation());
  EXPECT_EQ(PathValidationReason::kMultiPort,
            path_validator_.GetPathValidationReason());
  EXPECT_TRUE(path_validator_.IsValidatingPeerAddress(effective_peer_address_));
  EXPECT_CALL(*result_delegate_, OnPathValidationSuccess(_, _))
      .WillOnce(
          Invoke([=, this](std::unique_ptr<QuicPathValidationContext> context,
                           QuicTime start_time) {
            EXPECT_EQ(context.get(), context_);
            EXPECT_EQ(start_time, expected_start_time);
          }));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(kInitialRttMs));
  path_validator_.OnPathResponse(challenge_data, self_address_);
  EXPECT_FALSE(path_validator_.HasPendingPathValidation());
  EXPECT_EQ(PathValidationReason::kReasonUnknown,
            path_validator_.GetPathValidationReason());
}

TEST_F(QuicPathValidatorTest, RespondWithDifferentSelfAddress) {
  QuicPathFrameBuffer challenge_data;
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        memcpy(challenge_data.data(), payload.data(), payload.size());
        return true;
      }));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_));
  const QuicTime expected_start_time = clock_.Now();
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);

  // Reception of a PATH_RESPONSE on a different self address should be ignored.
  const QuicSocketAddress kAlternativeSelfAddress(QuicIpAddress::Any6(), 54321);
  EXPECT_NE(kAlternativeSelfAddress, self_address_);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(kInitialRttMs));
  path_validator_.OnPathResponse(challenge_data, kAlternativeSelfAddress);

  EXPECT_CALL(*result_delegate_, OnPathValidationSuccess(_, _))
      .WillOnce(
          Invoke([=, this](std::unique_ptr<QuicPathValidationContext> context,
                           QuicTime start_time) {
            EXPECT_EQ(context->self_address(), self_address_);
            EXPECT_EQ(start_time, expected_start_time);
          }));
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(kInitialRttMs));
  path_validator_.OnPathResponse(challenge_data, self_address_);
  EXPECT_EQ(PathValidationReason::kReasonUnknown,
            path_validator_.GetPathValidationReason());
}

TEST_F(QuicPathValidatorTest, RespondAfter1stRetry) {
  QuicPathFrameBuffer challenge_data;
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer& payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        // Store up the 1st PATH_CHALLANGE payload.
        memcpy(challenge_data.data(), payload.data(), payload.size());
        return true;
      }))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer& payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        EXPECT_NE(payload, challenge_data);
        return true;
      }));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_))
      .Times(2u);
  const QuicTime start_time = clock_.Now();
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  random_.ChangeValue();
  alarm_factory_.FireAlarm(
      QuicPathValidatorPeer::retry_timer(&path_validator_));

  EXPECT_CALL(*result_delegate_, OnPathValidationSuccess(_, start_time));
  // Respond to the 1st PATH_CHALLENGE should complete the validation.
  path_validator_.OnPathResponse(challenge_data, self_address_);
  EXPECT_FALSE(path_validator_.HasPendingPathValidation());
}

TEST_F(QuicPathValidatorTest, RespondToRetryChallenge) {
  QuicPathFrameBuffer challenge_data;
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer& payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        memcpy(challenge_data.data(), payload.data(), payload.size());
        return true;
      }))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer& payload,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           const QuicSocketAddress&, QuicPacketWriter*) {
        EXPECT_NE(challenge_data, payload);
        memcpy(challenge_data.data(), payload.data(), payload.size());
        return true;
      }));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_))
      .Times(2u);
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);

  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
  const QuicTime start_time = clock_.Now();
  random_.ChangeValue();
  alarm_factory_.FireAlarm(
      QuicPathValidatorPeer::retry_timer(&path_validator_));

  // Respond to the 2nd PATH_CHALLENGE should complete the validation.
  EXPECT_CALL(*result_delegate_, OnPathValidationSuccess(_, start_time));
  path_validator_.OnPathResponse(challenge_data, self_address_);
  EXPECT_FALSE(path_validator_.HasPendingPathValidation());
}

TEST_F(QuicPathValidatorTest, ValidationTimeOut) {
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .Times(3u)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_))
      .Times(3u);
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);

  QuicPathFrameBuffer challenge_data;
  memset(challenge_data.data(), 'a', challenge_data.size());
  // Reception of a PATH_RESPONSE with different payload should be ignored.
  path_validator_.OnPathResponse(challenge_data, self_address_);

  // Retry 3 times. The 3rd time should fail the validation.
  EXPECT_CALL(*result_delegate_, OnPathValidationFailure(_))
      .WillOnce(
          Invoke([=, this](std::unique_ptr<QuicPathValidationContext> context) {
            EXPECT_EQ(context_, context.get());
          }));
  for (size_t i = 0; i <= QuicPathValidator::kMaxRetryTimes; ++i) {
    clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(3 * kInitialRttMs));
    alarm_factory_.FireAlarm(
        QuicPathValidatorPeer::retry_timer(&path_validator_));
  }
  EXPECT_EQ(PathValidationReason::kReasonUnknown,
            path_validator_.GetPathValidationReason());
}

TEST_F(QuicPathValidatorTest, SendPathChallengeError) {
  EXPECT_CALL(send_delegate_,
              SendPathChallenge(_, self_address_, peer_address_,
                                effective_peer_address_, &writer_))
      .WillOnce(Invoke([&](const QuicPathFrameBuffer&, const QuicSocketAddress&,
                           const QuicSocketAddress&, const QuicSocketAddress&,
                           QuicPacketWriter*) {
        // Abandon this validation in the call stack shouldn't cause crash and
        // should cancel the alarm.
        path_validator_.CancelPathValidation();
        return false;
      }));
  EXPECT_CALL(send_delegate_, GetRetryTimeout(peer_address_, &writer_))
      .Times(0u);
  EXPECT_CALL(*result_delegate_, OnPathValidationFailure(_));
  path_validator_.StartPathValidation(
      std::unique_ptr<QuicPathValidationContext>(context_),
      std::unique_ptr<MockQuicPathValidationResultDelegate>(result_delegate_),
      PathValidationReason::kMultiPort);
  EXPECT_FALSE(path_validator_.HasPendingPathValidation());
  EXPECT_FALSE(QuicPathValidatorPeer::retry_timer(&path_validator_)->IsSet());
  EXPECT_EQ(PathValidationReason::kReasonUnknown,
            path_validator_.GetPathValidationReason());
}

}  // namespace test
}  // namespace quic
