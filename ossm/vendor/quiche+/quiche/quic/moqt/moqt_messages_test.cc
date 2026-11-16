// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_messages.h"

#include <cstdint>
#include <initializer_list>
#include <optional>

#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

using ParameterMutator = void (*)(MessageParameters&);

void SetSubgroupDeliveryTimeout(MessageParameters& p) {
  p.subgroup_delivery_timeout = quic::QuicTimeDelta::FromSeconds(1);
}
void SetAuthorizationTokens(MessageParameters& p) {
  p.authorization_tokens = {AuthToken(AuthTokenType::kOutOfBand, "token")};
}
void SetRendezvousTimeout(MessageParameters& p) {
  p.rendezvous_timeout = quic::QuicTimeDelta::FromSeconds(2);
}
void SetObjectDeliveryTimeout(MessageParameters& p) {
  p.object_delivery_timeout = quic::QuicTimeDelta::FromSeconds(3);
}
void SetExpires(MessageParameters& p) {
  p.expires = quic::QuicTimeDelta::FromSeconds(4);
}
void SetLargestObject(MessageParameters& p) {
  p.largest_object = Location(5, 6);
}
void SetFillTimeout(MessageParameters& p) {
  p.fill_timeout = quic::QuicTimeDelta::FromSeconds(7);
}
void SetForward(MessageParameters& p) { p.set_forward(true); }
void SetSubscriberPriority(MessageParameters& p) { p.subscriber_priority = 8; }
void SetLocationFilter(MessageParameters& p) {
  p.location_filter = LocationFilter::RelativeStartFilter(0);
}
void SetGroupOrder(MessageParameters& p) {
  p.group_order = MoqtGroupOrder::kAscending;
}
void SetNewGroupRequest(MessageParameters& p) { p.new_group_request = 9; }
void SetTrackNamespacePrefix(MessageParameters& p) {
  p.track_namespace_prefix = TrackNamespace({"foo"});
}
void SetOackWindowSize(MessageParameters& p) {
  p.oack_window_size = quic::QuicTimeDelta::FromSeconds(10);
}

constexpr ParameterMutator kAllParameterMutators[] = {
    SetSubgroupDeliveryTimeout,
    SetAuthorizationTokens,
    SetRendezvousTimeout,
    SetObjectDeliveryTimeout,
    SetExpires,
    SetLargestObject,
    SetFillTimeout,
    SetForward,
    SetSubscriberPriority,
    SetLocationFilter,
    SetGroupOrder,
    SetNewGroupRequest,
    SetTrackNamespacePrefix,
    SetOackWindowSize,
};

MessageParameters MakeParameters(
    std::initializer_list<ParameterMutator> mutators) {
  MessageParameters params;
  for (ParameterMutator mutate : mutators) {
    mutate(params);
  }
  return params;
}

MessageParameters AllParameters() {
  MessageParameters params;
  for (ParameterMutator mutate : kAllParameterMutators) {
    mutate(params);
  }
  return params;
}

template <typename AllowedFn, typename SanitizeFn>
void VerifyParameterHandling(MoqtMessageType type,
                             const MessageParameters& expected,
                             AllowedFn allowed_fn, SanitizeFn sanitize_fn) {
  SCOPED_TRACE(MoqtMessageTypeToString(type));
  MessageParameters sanitized = AllParameters();
  // No message can accept all parameters.
  EXPECT_FALSE(allowed_fn(sanitized, type));
  sanitize_fn(sanitized, type);
  EXPECT_EQ(sanitized, expected);
  EXPECT_TRUE(allowed_fn(MessageParameters(), type));
  EXPECT_TRUE(allowed_fn(expected, type));

  for (ParameterMutator mutate : kAllParameterMutators) {
    MessageParameters single_param;
    mutate(single_param);

    MessageParameters with_param = expected;
    mutate(with_param);
    const bool is_allowed = (with_param == expected);
    EXPECT_EQ(allowed_fn(single_param, type), is_allowed);
    EXPECT_EQ(allowed_fn(with_param, type), is_allowed);
  }
}

TEST(MoqtMessagesTest, MoqtDatagramType) {
  for (bool payload : {false, true}) {
    for (bool properties : {false, true}) {
      for (bool end_of_group : {false, true}) {
        for (bool default_priority : {false, true}) {
          for (bool zero_object_id : {false, true}) {
            MoqtDatagramType type(payload, properties, end_of_group,
                                  default_priority, zero_object_id);
            EXPECT_EQ(type.has_status(), !payload && !properties);
            EXPECT_EQ(type.has_properties(), properties);
            EXPECT_EQ(type.end_of_group(),
                      end_of_group && (payload || properties));
            EXPECT_EQ(type.has_object_id(), !zero_object_id);
            EXPECT_EQ(type.has_default_priority(), default_priority);
            // The constructor should always produce a valid value.
            std::optional<MoqtDatagramType> from_value =
                MoqtDatagramType::FromValue(type.value());
            EXPECT_TRUE(from_value.has_value() && type == *from_value);
          }
        }
      }
    }
  }
}

TEST(MoqtMessagesTest, ParametersAllowedAndSanitizedByMessage) {
  VerifyParameterHandling(
      MoqtMessageType::kSubscribe,
      MakeParameters({SetSubgroupDeliveryTimeout, SetAuthorizationTokens,
                      SetRendezvousTimeout, SetObjectDeliveryTimeout,
                      SetForward, SetSubscriberPriority, SetLocationFilter,
                      SetGroupOrder, SetNewGroupRequest, SetOackWindowSize}),
      ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(MoqtMessageType::kSubscribeOk,
                          MakeParameters({SetExpires, SetLargestObject}),
                          ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(MoqtMessageType::kPublish,
                          MakeParameters({SetAuthorizationTokens, SetExpires,
                                          SetLargestObject, SetForward}),
                          ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(
      MoqtMessageType::kPublishStateNotify,
      MakeParameters({SetLargestObject, SetForward, SetLocationFilter}),
      ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(
      MoqtMessageType::kFetch,
      MakeParameters({SetAuthorizationTokens, SetFillTimeout,
                      SetSubscriberPriority, SetGroupOrder}),
      ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(MoqtMessageType::kFetchOk, MessageParameters(),
                          ParametersAllowedByMessage, SanitizeParameters);

  VerifyParameterHandling(MoqtMessageType::kSubscribeTracks,
                          MakeParameters({SetAuthorizationTokens, SetForward}),
                          ParametersAllowedByMessage, SanitizeParameters);

  for (MoqtMessageType type :
       {MoqtMessageType::kTrackStatus, MoqtMessageType::kPublishNamespace,
        MoqtMessageType::kSubscribeNamespace}) {
    VerifyParameterHandling(type, MakeParameters({SetAuthorizationTokens}),
                            ParametersAllowedByMessage, SanitizeParameters);
  }

  for (MoqtMessageType invalid_type :
       {MoqtMessageType::kRequestUpdate, MoqtMessageType::kRequestError,
        MoqtMessageType::kRequestOk, MoqtMessageType::kNamespace,
        MoqtMessageType::kPublishDone, MoqtMessageType::kNamespaceDone,
        MoqtMessageType::kPublishSkipped, MoqtMessageType::kGoAway,
        MoqtMessageType::kSetup, MoqtMessageType::kObjectAck}) {
    MessageParameters params = AllParameters();
    EXPECT_QUICHE_BUG(
        EXPECT_FALSE(ParametersAllowedByMessage(params, invalid_type)),
        "Unexpected message type");
    EXPECT_QUICHE_BUG(SanitizeParameters(params, invalid_type),
                      "Unexpected message type");
    EXPECT_EQ(params, MessageParameters());
  }
}

TEST(MoqtMessagesTest, ParametersAllowedAndSanitizedByRequestUpdate) {
  for (MoqtMessageType type :
       {MoqtMessageType::kSubscribe, MoqtMessageType::kRequestOk}) {
    VerifyParameterHandling(
        type,
        MakeParameters({SetSubgroupDeliveryTimeout, SetAuthorizationTokens,
                        SetObjectDeliveryTimeout, SetForward,
                        SetSubscriberPriority, SetLocationFilter,
                        SetNewGroupRequest, SetOackWindowSize}),
        ParametersAllowedByRequestUpdate, SanitizeUpdateParameters);
  }

  VerifyParameterHandling(
      MoqtMessageType::kFetch,
      MakeParameters({SetAuthorizationTokens, SetSubscriberPriority}),
      ParametersAllowedByRequestUpdate, SanitizeUpdateParameters);

  for (MoqtMessageType type :
       {MoqtMessageType::kPublish, MoqtMessageType::kPublishNamespace}) {
    VerifyParameterHandling(type, MakeParameters({SetAuthorizationTokens}),
                            ParametersAllowedByRequestUpdate,
                            SanitizeUpdateParameters);
  }

  for (MoqtMessageType type : {MoqtMessageType::kSubscribeNamespace,
                               MoqtMessageType::kSubscribeTracks}) {
    VerifyParameterHandling(
        type, MakeParameters({SetAuthorizationTokens, SetTrackNamespacePrefix}),
        ParametersAllowedByRequestUpdate, SanitizeUpdateParameters);
  }

  for (MoqtMessageType invalid_type :
       {MoqtMessageType::kRequestUpdate, MoqtMessageType::kSubscribeOk,
        MoqtMessageType::kRequestError, MoqtMessageType::kNamespace,
        MoqtMessageType::kPublishDone, MoqtMessageType::kTrackStatus,
        MoqtMessageType::kNamespaceDone, MoqtMessageType::kPublishSkipped,
        MoqtMessageType::kGoAway, MoqtMessageType::kFetchOk,
        MoqtMessageType::kPublishStateNotify, MoqtMessageType::kSetup,
        MoqtMessageType::kObjectAck}) {
    MessageParameters params = AllParameters();
    EXPECT_QUICHE_BUG(
        EXPECT_FALSE(ParametersAllowedByRequestUpdate(params, invalid_type)),
        "Unexpected updated message type");
    EXPECT_QUICHE_BUG(SanitizeUpdateParameters(params, invalid_type),
                      "Unexpected updated message type");
    EXPECT_EQ(params, MessageParameters());
  }
}

TEST(MoqtMessagesTest, ParametersAllowedAndSanitizedByRequestOk) {
  VerifyParameterHandling(
      MoqtMessageType::kPublish,
      MakeParameters({SetSubgroupDeliveryTimeout, SetObjectDeliveryTimeout,
                      SetExpires, SetForward, SetSubscriberPriority,
                      SetLocationFilter, SetGroupOrder, SetNewGroupRequest,
                      SetOackWindowSize}),
      ParametersAllowedByRequestOk, SanitizeRequestOkParameters);

  VerifyParameterHandling(MoqtMessageType::kRequestUpdate,
                          MakeParameters({SetExpires, SetLargestObject}),
                          ParametersAllowedByRequestOk,
                          SanitizeRequestOkParameters);

  VerifyParameterHandling(
      MoqtMessageType::kTrackStatus, MakeParameters({SetLargestObject}),
      ParametersAllowedByRequestOk, SanitizeRequestOkParameters);

  for (MoqtMessageType type : {MoqtMessageType::kPublishNamespace,
                               MoqtMessageType::kSubscribeNamespace,
                               MoqtMessageType::kSubscribeTracks}) {
    VerifyParameterHandling(type, MessageParameters(),
                            ParametersAllowedByRequestOk,
                            SanitizeRequestOkParameters);
  }

  for (MoqtMessageType invalid_type :
       {MoqtMessageType::kSubscribe, MoqtMessageType::kSubscribeOk,
        MoqtMessageType::kRequestError, MoqtMessageType::kRequestOk,
        MoqtMessageType::kNamespace, MoqtMessageType::kPublishDone,
        MoqtMessageType::kNamespaceDone, MoqtMessageType::kPublishSkipped,
        MoqtMessageType::kGoAway, MoqtMessageType::kFetch,
        MoqtMessageType::kFetchOk, MoqtMessageType::kPublishStateNotify,
        MoqtMessageType::kSetup, MoqtMessageType::kObjectAck}) {
    MessageParameters params = AllParameters();
    EXPECT_QUICHE_BUG(
        EXPECT_FALSE(ParametersAllowedByRequestOk(params, invalid_type)),
        "Unexpected request type for REQUEST_OK");
    EXPECT_QUICHE_BUG(SanitizeRequestOkParameters(params, invalid_type),
                      "Unexpected request type for REQUEST_OK");
    EXPECT_EQ(params, MessageParameters());
  }
}

TEST(MoqtMessagesTest, RedirectAllowedByRequestError) {
  std::optional<Redirect> no_redirect = std::nullopt;
  std::optional<Redirect> empty_redirect = Redirect{"", FullTrackName()};
  std::optional<Redirect> namespace_only_redirect = Redirect{
      "moqt://example.com", FullTrackName(TrackNamespace({"foo"}), "")};
  std::optional<Redirect> full_track_redirect =
      Redirect{"moqt://example.com", FullTrackName("foo", "bar")};

  for (MoqtMessageType type : {MoqtMessageType::kPublishNamespace,
                               MoqtMessageType::kSubscribeNamespace}) {
    EXPECT_TRUE(RedirectAllowedByRequestError(no_redirect, type));
    EXPECT_TRUE(RedirectAllowedByRequestError(empty_redirect, type));
    EXPECT_TRUE(RedirectAllowedByRequestError(namespace_only_redirect, type));
    EXPECT_FALSE(RedirectAllowedByRequestError(full_track_redirect, type));
  }

  for (MoqtMessageType type :
       {MoqtMessageType::kSubscribe, MoqtMessageType::kFetch,
        MoqtMessageType::kTrackStatus}) {
    EXPECT_TRUE(RedirectAllowedByRequestError(no_redirect, type));
    EXPECT_TRUE(RedirectAllowedByRequestError(empty_redirect, type));
    EXPECT_TRUE(RedirectAllowedByRequestError(namespace_only_redirect, type));
    EXPECT_TRUE(RedirectAllowedByRequestError(full_track_redirect, type));
  }

  for (MoqtMessageType type :
       {MoqtMessageType::kPublish, MoqtMessageType::kRequestUpdate,
        MoqtMessageType::kSubscribeTracks}) {
    EXPECT_TRUE(RedirectAllowedByRequestError(no_redirect, type));
    EXPECT_FALSE(RedirectAllowedByRequestError(empty_redirect, type));
    EXPECT_FALSE(RedirectAllowedByRequestError(namespace_only_redirect, type));
    EXPECT_FALSE(RedirectAllowedByRequestError(full_track_redirect, type));
  }

  for (MoqtMessageType invalid_type :
       {MoqtMessageType::kSubscribeOk, MoqtMessageType::kRequestError,
        MoqtMessageType::kRequestOk, MoqtMessageType::kNamespace,
        MoqtMessageType::kPublishDone, MoqtMessageType::kNamespaceDone,
        MoqtMessageType::kPublishSkipped, MoqtMessageType::kGoAway,
        MoqtMessageType::kFetchOk, MoqtMessageType::kPublishStateNotify,
        MoqtMessageType::kSetup, MoqtMessageType::kObjectAck}) {
    EXPECT_QUICHE_BUG(
        EXPECT_FALSE(RedirectAllowedByRequestError(no_redirect, invalid_type)),
        "Unexpected request type for REQUEST_ERROR");
  }
}

TEST(MoqtMessagesTest, IntegerToObjectStatus) {
  EXPECT_EQ(IntegerToObjectStatus(0), MoqtObjectStatus::kNormal);
  EXPECT_EQ(IntegerToObjectStatus(1), MoqtObjectStatus::kInvalidObjectStatus);
  EXPECT_EQ(IntegerToObjectStatus(2), MoqtObjectStatus::kInvalidObjectStatus);
  EXPECT_EQ(IntegerToObjectStatus(3), MoqtObjectStatus::kEndOfGroup);
  EXPECT_EQ(IntegerToObjectStatus(4), MoqtObjectStatus::kEndOfTrack);
  EXPECT_EQ(IntegerToObjectStatus(5), MoqtObjectStatus::kInvalidObjectStatus);
  EXPECT_EQ(IntegerToObjectStatus(UINT64_MAX),
            MoqtObjectStatus::kInvalidObjectStatus);
}

}  // namespace
}  // namespace moqt::test
