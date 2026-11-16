// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_messages.h"

#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"

namespace moqt {

namespace {

struct AllowedParameters {
  bool subgroup_delivery_timeout = false;
  bool authorization_tokens = false;
  bool rendezvous_timeout = false;
  bool object_delivery_timeout = false;
  bool expires = false;
  bool largest_object = false;
  bool fill_timeout = false;
  bool forward = false;
  bool subscriber_priority = false;
  bool location_filter = false;
  bool group_order = false;
  bool new_group_request = false;
  bool track_namespace_prefix = false;
  bool oack_window_size = false;
};

constexpr AllowedParameters kCommonSubscriberParameters{
    .subgroup_delivery_timeout = true,
    .object_delivery_timeout = true,
    .forward = true,
    .subscriber_priority = true,
    .location_filter = true,
    .new_group_request = true,
    .oack_window_size = true,
};

AllowedParameters GetAllowedParametersForMessage(MoqtMessageType message_type) {
  switch (message_type) {
    case MoqtMessageType::kSubscribe: {
      AllowedParameters allowed = kCommonSubscriberParameters;
      allowed.authorization_tokens = true;
      allowed.rendezvous_timeout = true;
      allowed.group_order = true;
      return allowed;
    }
    case MoqtMessageType::kSubscribeOk:
      return AllowedParameters{
          .expires = true,
          .largest_object = true,
      };
    case MoqtMessageType::kPublish:
      return AllowedParameters{
          .authorization_tokens = true,
          .expires = true,
          .largest_object = true,
          .forward = true,
      };
    case MoqtMessageType::kPublishStateNotify:
      return AllowedParameters{
          .largest_object = true,
          .forward = true,
          .location_filter = true,
      };
    case MoqtMessageType::kFetch:
      return AllowedParameters{
          .authorization_tokens = true,
          .fill_timeout = true,
          .subscriber_priority = true,
          .group_order = true,
      };
    case MoqtMessageType::kFetchOk:
      return AllowedParameters{};
    case MoqtMessageType::kSubscribeTracks:
      return AllowedParameters{
          .authorization_tokens = true,
          .forward = true,
      };
    case MoqtMessageType::kTrackStatus:
    case MoqtMessageType::kPublishNamespace:
    case MoqtMessageType::kSubscribeNamespace:
      return AllowedParameters{
          .authorization_tokens = true,
      };
    case MoqtMessageType::kRequestUpdate:
    case MoqtMessageType::kRequestError:
    case MoqtMessageType::kRequestOk:
    case MoqtMessageType::kNamespace:
    case MoqtMessageType::kPublishDone:
    case MoqtMessageType::kNamespaceDone:
    case MoqtMessageType::kPublishSkipped:
    case MoqtMessageType::kGoAway:
    case MoqtMessageType::kSetup:
    case MoqtMessageType::kObjectAck:
      QUICHE_BUG(moqt_sanitize_parameters_invalid_type)
          << "Unexpected message type: "
          << MoqtMessageTypeToString(message_type);
      return AllowedParameters{};
  }
  return AllowedParameters{};
}

AllowedParameters GetAllowedParametersForUpdate(MoqtMessageType updated_type) {
  switch (updated_type) {
    case MoqtMessageType::kSubscribe:
    case MoqtMessageType::kRequestOk: {  // PUBLISH_OK
      AllowedParameters allowed = kCommonSubscriberParameters;
      allowed.authorization_tokens = true;
      return allowed;
    }
    case MoqtMessageType::kFetch:
      return AllowedParameters{
          .authorization_tokens = true,
          .subscriber_priority = true,
      };
    case MoqtMessageType::kPublish:
    case MoqtMessageType::kPublishNamespace:
      return AllowedParameters{
          .authorization_tokens = true,
      };
    case MoqtMessageType::kSubscribeNamespace:
    case MoqtMessageType::kSubscribeTracks:
      return AllowedParameters{
          .authorization_tokens = true,
          .track_namespace_prefix = true,
      };
    case MoqtMessageType::kRequestUpdate:
    case MoqtMessageType::kSubscribeOk:
    case MoqtMessageType::kRequestError:
    case MoqtMessageType::kNamespace:
    case MoqtMessageType::kPublishDone:
    case MoqtMessageType::kTrackStatus:
    case MoqtMessageType::kNamespaceDone:
    case MoqtMessageType::kPublishSkipped:
    case MoqtMessageType::kGoAway:
    case MoqtMessageType::kFetchOk:
    case MoqtMessageType::kPublishStateNotify:
    case MoqtMessageType::kSetup:
    case MoqtMessageType::kObjectAck:
      QUICHE_BUG(moqt_sanitize_update_parameters_invalid_type)
          << "Unexpected updated message type: "
          << MoqtMessageTypeToString(updated_type);
      return AllowedParameters{};
  }
  return AllowedParameters{};
}

AllowedParameters GetAllowedParametersForRequestOk(MoqtMessageType type_of_ok) {
  switch (type_of_ok) {
    case MoqtMessageType::kPublish: {
      AllowedParameters allowed = kCommonSubscriberParameters;
      allowed.expires = true;
      allowed.group_order = true;
      return allowed;
    }
    case MoqtMessageType::kRequestUpdate:
      return AllowedParameters{
          .expires = true,
          .largest_object = true,
      };
    case MoqtMessageType::kTrackStatus:
      return AllowedParameters{
          .largest_object = true,
      };
    case MoqtMessageType::kPublishNamespace:
    case MoqtMessageType::kSubscribeNamespace:
    case MoqtMessageType::kSubscribeTracks:
      return AllowedParameters{};
    case MoqtMessageType::kSubscribe:
    case MoqtMessageType::kSubscribeOk:
    case MoqtMessageType::kRequestError:
    case MoqtMessageType::kRequestOk:
    case MoqtMessageType::kNamespace:
    case MoqtMessageType::kPublishDone:
    case MoqtMessageType::kNamespaceDone:
    case MoqtMessageType::kPublishSkipped:
    case MoqtMessageType::kGoAway:
    case MoqtMessageType::kFetch:
    case MoqtMessageType::kFetchOk:
    case MoqtMessageType::kPublishStateNotify:
    case MoqtMessageType::kSetup:
    case MoqtMessageType::kObjectAck:
      QUICHE_BUG(moqt_sanitize_request_ok_parameters_invalid_type)
          << "Unexpected request type for REQUEST_OK: "
          << MoqtMessageTypeToString(type_of_ok);
      return AllowedParameters{};
  }
  return AllowedParameters{};
}

bool CheckAllowedParameters(const MessageParameters& parameters,
                            const AllowedParameters& allowed) {
  return (allowed.subgroup_delivery_timeout ||
          !parameters.subgroup_delivery_timeout.has_value()) &&
         (allowed.authorization_tokens ||
          parameters.authorization_tokens.empty()) &&
         (allowed.rendezvous_timeout ||
          !parameters.rendezvous_timeout.has_value()) &&
         (allowed.object_delivery_timeout ||
          !parameters.object_delivery_timeout.has_value()) &&
         (allowed.expires || !parameters.expires.has_value()) &&
         (allowed.largest_object || !parameters.largest_object.has_value()) &&
         (allowed.fill_timeout || !parameters.fill_timeout.has_value()) &&
         (allowed.forward || !parameters.forward_has_value()) &&
         (allowed.subscriber_priority ||
          !parameters.subscriber_priority.has_value()) &&
         (allowed.location_filter || !parameters.location_filter.has_value()) &&
         (allowed.group_order || !parameters.group_order.has_value()) &&
         (allowed.new_group_request ||
          !parameters.new_group_request.has_value()) &&
         (allowed.track_namespace_prefix ||
          !parameters.track_namespace_prefix.has_value()) &&
         (allowed.oack_window_size || !parameters.oack_window_size.has_value());
}

void ApplyAllowedParameters(MessageParameters& parameters,
                            const AllowedParameters& allowed) {
  if (!allowed.subgroup_delivery_timeout) {
    parameters.subgroup_delivery_timeout.reset();
  }
  if (!allowed.authorization_tokens) {
    parameters.authorization_tokens.clear();
  }
  if (!allowed.rendezvous_timeout) {
    parameters.rendezvous_timeout.reset();
  }
  if (!allowed.object_delivery_timeout) {
    parameters.object_delivery_timeout.reset();
  }
  if (!allowed.expires) {
    parameters.expires.reset();
  }
  if (!allowed.largest_object) {
    parameters.largest_object.reset();
  }
  if (!allowed.fill_timeout) {
    parameters.fill_timeout.reset();
  }
  if (!allowed.forward) {
    parameters.clear_forward();
  }
  if (!allowed.subscriber_priority) {
    parameters.subscriber_priority.reset();
  }
  if (!allowed.location_filter) {
    parameters.location_filter.reset();
  }
  if (!allowed.group_order) {
    parameters.group_order.reset();
  }
  if (!allowed.new_group_request) {
    parameters.new_group_request.reset();
  }
  if (!allowed.track_namespace_prefix) {
    parameters.track_namespace_prefix.reset();
  }
  if (!allowed.oack_window_size) {
    parameters.oack_window_size.reset();
  }
}

}  // namespace

MoqtObjectStatus IntegerToObjectStatus(uint64_t integer) {
  switch (integer) {
    case 0x0:
      return MoqtObjectStatus::kNormal;
    case 0x3:
      return MoqtObjectStatus::kEndOfGroup;
    case 0x4:
      return MoqtObjectStatus::kEndOfTrack;
    default:
      return MoqtObjectStatus::kInvalidObjectStatus;
  }
}

MoqtError SetupOptionsAllowedByMessage(const SetupOptions& options,
                                       quic::Perspective sender_perspective,
                                       bool webtrans) {
  bool should_have_path_and_authority =
      !webtrans && sender_perspective == quic::Perspective::IS_CLIENT;
  if (should_have_path_and_authority != options.path.has_value()) {
    return MoqtError::kInvalidPath;
  }
  if (should_have_path_and_authority != options.authority.has_value()) {
    return MoqtError::kInvalidAuthority;
  }
  return MoqtError::kNoError;
}

bool ParametersAllowedByMessage(const MessageParameters& parameters,
                                MoqtMessageType message_type) {
  return CheckAllowedParameters(parameters,
                                GetAllowedParametersForMessage(message_type));
}

void SanitizeParameters(MessageParameters& parameters,
                        MoqtMessageType message_type) {
  ApplyAllowedParameters(parameters,
                         GetAllowedParametersForMessage(message_type));
}

bool ParametersAllowedByRequestUpdate(const MessageParameters& parameters,
                                      MoqtMessageType updated_type) {
  return CheckAllowedParameters(parameters,
                                GetAllowedParametersForUpdate(updated_type));
}

void SanitizeUpdateParameters(MessageParameters& parameters,
                              MoqtMessageType updated_type) {
  ApplyAllowedParameters(parameters,
                         GetAllowedParametersForUpdate(updated_type));
}

bool ParametersAllowedByRequestOk(const MessageParameters& parameters,
                                  MoqtMessageType type_of_ok) {
  return CheckAllowedParameters(parameters,
                                GetAllowedParametersForRequestOk(type_of_ok));
}

void SanitizeRequestOkParameters(MessageParameters& parameters,
                                 MoqtMessageType type_of_ok) {
  ApplyAllowedParameters(parameters,
                         GetAllowedParametersForRequestOk(type_of_ok));
}

bool RedirectAllowedByRequestError(const std::optional<Redirect>& redirect,
                                   MoqtMessageType request_type) {
  switch (request_type) {
    case MoqtMessageType::kPublishNamespace:
    case MoqtMessageType::kSubscribeNamespace:
      return !redirect.has_value() || redirect->full_track_name.name().empty();
    case MoqtMessageType::kSubscribe:
    case MoqtMessageType::kFetch:
    case MoqtMessageType::kTrackStatus:
      return true;
    case MoqtMessageType::kPublish:
    case MoqtMessageType::kRequestUpdate:
    case MoqtMessageType::kSubscribeTracks:
      return !redirect.has_value();
    case MoqtMessageType::kSubscribeOk:
    case MoqtMessageType::kRequestError:
    case MoqtMessageType::kRequestOk:
    case MoqtMessageType::kNamespace:
    case MoqtMessageType::kPublishDone:
    case MoqtMessageType::kNamespaceDone:
    case MoqtMessageType::kPublishSkipped:
    case MoqtMessageType::kGoAway:
    case MoqtMessageType::kFetchOk:
    case MoqtMessageType::kPublishStateNotify:
    case MoqtMessageType::kSetup:
    case MoqtMessageType::kObjectAck:
      QUICHE_BUG(moqt_redirect_allowed_invalid_type)
          << "Unexpected request type for REQUEST_ERROR: "
          << MoqtMessageTypeToString(request_type);
      return false;
  }
  return false;
}

std::string MoqtMessageTypeToString(const MoqtMessageType message_type) {
  switch (message_type) {
    case MoqtMessageType::kSetup:
      return "SETUP";
    case MoqtMessageType::kSubscribe:
      return "SUBSCRIBE";
    case MoqtMessageType::kSubscribeOk:
      return "SUBSCRIBE_OK";
    case MoqtMessageType::kRequestError:
      return "REQUEST_ERROR";
    case MoqtMessageType::kPublishDone:
      return "PUBLISH_DONE";
    case MoqtMessageType::kRequestUpdate:
      return "REQUEST_UPDATE";
    case MoqtMessageType::kTrackStatus:
      return "TRACK_STATUS";
    case MoqtMessageType::kPublishNamespace:
      return "PUBLISH_NAMESPACE";
    case MoqtMessageType::kNamespace:
      return "NAMESPACE";
    case MoqtMessageType::kNamespaceDone:
      return "NAMESPACE_DONE";
    case MoqtMessageType::kPublishSkipped:
      return "PUBLISH_SKIPPED";
    case MoqtMessageType::kRequestOk:
      return "REQUEST_OK";
    case MoqtMessageType::kGoAway:
      return "GOAWAY";
    case MoqtMessageType::kSubscribeNamespace:
      return "SUBSCRIBE_NAMESPACE";
    case MoqtMessageType::kSubscribeTracks:
      return "SUBSCRIBE_TRACKS";
    case MoqtMessageType::kPublish:
      return "PUBLISH";
    case MoqtMessageType::kPublishStateNotify:
      return "PUBLISH_STATE_NOTIFY";
    case MoqtMessageType::kFetch:
      return "FETCH";
    case MoqtMessageType::kFetchOk:
      return "FETCH_OK";
    case MoqtMessageType::kObjectAck:
      return "OBJECT_ACK";
  }
  return "Unknown message " + std::to_string(static_cast<int>(message_type));
}

std::string MoqtDataStreamTypeToString(MoqtDataStreamType type) {
  return type.IsFetch() ? "STREAM_HEADER_FETCH"
                        : absl::StrCat("STREAM_HEADER_SUBGROUP_", type.value());
}

std::string MoqtDatagramTypeToString(MoqtDatagramType type) {
  return absl::StrCat("DATAGRAM", type.has_status() ? "_STATUS" : "",
                      type.has_properties() ? "_PROPERTIES" : "");
}

std::string MoqtFetchSerializationToString(MoqtFetchSerialization type) {
  return absl::StrCat("FETCH_SERIALIZATION_", type.value());
}

std::string MoqtForwardingPreferenceToString(
    MoqtForwardingPreference preference) {
  switch (preference) {
    case MoqtForwardingPreference::kDatagram:
      return "DATAGRAM";
    case MoqtForwardingPreference::kSubgroup:
      return "SUBGROUP";
  }
  QUICHE_BUG(quic_bug_bad_moqt_message_type_01)
      << "Unknown preference " << std::to_string(static_cast<int>(preference));
  return "Unknown preference " + std::to_string(static_cast<int>(preference));
}

}  // namespace moqt
