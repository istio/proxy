// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_publish_stream.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_clock.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/common/quiche_status_utils.h"

namespace moqt {

MoqtPublishRequestStream::MoqtPublishRequestStream(
    MoqtFramer* absl_nonnull framer,
    const MoqtControlMessageParser& message_parser,
    LivePublisher::RemoveCallback stream_deleted_callback,
    SessionErrorCallback session_error_callback,
    ValidateRequestIdCallback validate_request_id,
    MoqtResponseCallback response_callback)
    : MoqtBidiStreamBase(framer, message_parser,
                         std::move(session_error_callback)),
      response_callback_(std::move(response_callback)),
      stream_deleted_callback_(std::move(stream_deleted_callback)),
      validate_request_id_(std::move(validate_request_id)) {}

MoqtPublishRequestStream::~MoqtPublishRequestStream() {
  if (publisher_ != nullptr) {
    publisher_->IgnoreResetAllStreams();
  }
  Detach();
}

void MoqtPublishRequestStream::OnStreamBound() {
  publisher_->parameters().largest_object =
      publisher_->publisher().largest_location();
  publisher_->parameters().expires = publisher_->publisher().expiration();
  MoqtPublish publish{publisher_->request_id(),
                      publisher_->publisher().GetTrackName(),
                      publisher_->track_alias(), publisher_->parameters(),
                      publisher_->publisher().properties()};
  publish.parameters.group_order.reset();
  SendOrBufferMessageOrFatal(framer()->SerializePublish(publish));
}

absl::Status MoqtPublishRequestStream::OnRawControlMessage(
    const MoqtRawControlMessage& message) {
  return ControlMessageDispatcher::DispatchControlMessage(
      *this, message_parser(), message, "publish publisher");
}

absl::Status MoqtPublishRequestStream::OnControlMessage(
    const MoqtRequestOk& message) {
  if (!message.properties.empty()) {
    OnFatalError(
        absl::InvalidArgumentError("REQUEST_OK received with properties"));
    return absl::OkStatus();
  }
  if (response_callback_ != nullptr) {
    // PUBLISH_OK
    if (!ParametersAllowedByRequestOk(message.parameters,
                                      MoqtMessageType::kPublish)) {
      return absl::InvalidArgumentError(
          "REQUEST_OK contains invalid parameters for PUBLISH");
    }
    publisher_->Update(message.parameters, /*from_request_ok=*/true);
    // In draft-18, PUBLISH_OK can update the group order. This has been
    // eliminated since. This is not implemented because it is likely to be
    // buggy to change the group order mid-subscription.
    MoqtResponseCallback callback = std::move(response_callback_);
    response_callback_ = nullptr;
    std::move(callback)(message.parameters);
    return absl::OkStatus();
  }
  // REQUEST_UPDATE_OK
  if (!ParametersAllowedByRequestOk(message.parameters,
                                    MoqtMessageType::kRequestUpdate)) {
    return absl::InvalidArgumentError(
        "REQUEST_OK contains invalid parameters for REQUEST_UPDATE");
  }
  QUICHE_ASSIGN_OR_RETURN(MessageParameters parameters,
                          request_update_queue().NextParameters());
  // Apply the pending parameters to the subscription.
  publisher_->Update(parameters, /*from_request_ok=*/true);
  if (publisher_ != nullptr) {
    // Apply any parameters from the REQUEST_OK.
    publisher_->Update(message.parameters, /*from_request_ok=*/true);
  }
  return request_update_queue().OnControlMessage(message);
}

absl::Status MoqtPublishRequestStream::OnControlMessage(
    const MoqtRequestError& message) {
  if (response_callback_ != nullptr) {
    if (!RedirectAllowedByRequestError(message.redirect,
                                       MoqtMessageType::kPublish)) {
      return absl::InvalidArgumentError(
          "REQUEST_ERROR contains invalid redirect for PUBLISH");
    }
    std::move(response_callback_)(message);
    return absl::OkStatus();
  }
  if (!RedirectAllowedByRequestError(message.redirect,
                                     MoqtMessageType::kRequestUpdate)) {
    return absl::InvalidArgumentError(
        "REQUEST_ERROR contains invalid redirect for REQUEST_UPDATE");
  }
  return request_update_queue().OnControlMessage(message);
}

absl::Status MoqtPublishRequestStream::OnControlMessage(
    const MoqtRequestUpdate& message) {
  QUICHE_RETURN_IF_ERROR(validate_request_id_(message.request_id));
  if (!ParametersAllowedByRequestUpdate(message.parameters,
                                        MoqtMessageType::kRequestOk)) {
    return absl::InvalidArgumentError(
        "REQUEST_UPDATE contains invalid parameters for PUBLISH_OK");
  }
  publisher_->Update(message.parameters, /*from_request_ok=*/false);
  return absl::OkStatus();
}

MoqtPublishResponseStream::MoqtPublishResponseStream(
    MoqtFramer* absl_nonnull framer,
    const MoqtControlMessageParser& message_parser,
    const quic::QuicClock* absl_nonnull clock,
    quic::QuicAlarmFactory* absl_nonnull alarm_factory,
    SessionErrorCallback session_error_callback,
    ValidateRequestIdCallback validate_request_id,
    const MoqtIncomingPublishCallback* absl_nonnull incoming_publish_callback,
    LiveSubscriber::AddCallback add_callback,
    LiveSubscriber::RemoveCallback remove_callback)
    : MoqtBidiStreamBase(framer, message_parser,
                         std::move(session_error_callback)),
      clock_(clock),
      alarm_factory_(alarm_factory),
      validate_request_id_(std::move(validate_request_id)),
      incoming_publish_callback_(incoming_publish_callback),
      add_callback_(std::move(add_callback)),
      remove_callback_(std::move(remove_callback)),
      weak_ptr_factory_(this) {}

absl::Status MoqtPublishResponseStream::OnRawControlMessage(
    const MoqtRawControlMessage& message) {
  return ControlMessageDispatcher::DispatchControlMessage(
      *this, message_parser(), message, "publish subscriber");
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtPublish& message) {
  if (add_callback_ == nullptr) {
    // Two PUBLISH messages for the same stream.
    return absl::InvalidArgumentError("Multiple PUBLISH on the same stream");
  }
  QUICHE_RETURN_IF_ERROR(validate_request_id_(message.request_id));
  if (message.full_track_name.DoesNotExist()) {
    add_callback_ = nullptr;
    remove_callback_ = nullptr;
    return SendRequestError(RequestErrorCode::kDoesNotExist,
                            /*retry_interval=*/std::nullopt,
                            "Reserved track name");
  }
  absl::Status mandatory_property_status =
      message.properties.CheckForUnknownMandatoryProperty();
  if (!mandatory_property_status.ok()) {
    add_callback_ = nullptr;
    remove_callback_ = nullptr;
    return SendRequestError(
        StatusToMoqtRequestError(mandatory_property_status));
  }
  subscriber_ = std::make_unique<LiveSubscriber>(message, nullptr, this);
  if (!std::move(add_callback_)(subscriber_.get())) {
    add_callback_ = nullptr;
    return SendRequestError(RequestErrorCode::kDuplicateSubscription,
                            /*retry_interval=*/std::nullopt, "");
  }
  add_callback_ = nullptr;
  if (subscriber_->visitor() == nullptr) {
    // There was no existing SUBSCRIBE, so invoke the callback.
    subscriber_->set_visitor((*incoming_publish_callback_)(
        message.full_track_name, message.parameters, message.properties,
        [weakptr = weak_ptr_factory_.Create(),
         dynamic_groups = message.properties.dynamic_groups()](
            const std::variant<MessageParameters, MoqtRequestErrorInfo>
                response) {
          MoqtPublishResponseStream* stream = weakptr.GetIfAvailable();
          if (stream == nullptr) {
            return;
          }
          std::visit(
              absl::Overload{
                  [response_stream = stream,
                   dg = dynamic_groups](const MessageParameters& parameters) {
                    MessageParameters update_parameters = parameters;
                    if (!dg) {
                      update_parameters.new_group_request.reset();
                    }
                    if (!response_stream->CheckStatus(
                            response_stream->subscriber_->Update(
                                update_parameters))) {
                      return;
                    }
                    response_stream->CheckStatus(response_stream->SendRequestOk(
                        update_parameters, MoqtMessageType::kPublish));
                  },
                  [response_stream =
                       stream](const MoqtRequestErrorInfo& error_info) {
                    response_stream->CheckStatus(
                        response_stream->SendRequestError(error_info));
                  }},
              response);
        }));
  } else {
    // Since the application already called SUBSCRIBE, there will be no
    // invocation of the request callback. Send REQUEST_OK immediately.
    CheckStatus(SendRequestOk(subscriber_->const_parameters(),
                              MoqtMessageType::kPublish));
  }
  incoming_publish_callback_ = nullptr;
  if (subscriber_->visitor() == nullptr) {
    // The application doesn't care.
    CheckStatus(SendRequestError(RequestErrorCode::kUninterested,
                                 /*retry_interval=*/std::nullopt, ""));
    return absl::OkStatus();
  }
  // Notify the visitor.
  subscriber_->OnObjectOrOk(
      SubscribeOkData{message.parameters, message.properties});
  return absl::OkStatus();
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtRequestUpdate& message) {
  QUICHE_RETURN_IF_ERROR(validate_request_id_(message.request_id));
  if (!ParametersAllowedByRequestUpdate(message.parameters,
                                        MoqtMessageType::kPublish)) {
    return absl::InvalidArgumentError(
        "REQUEST_UPDATE contains invalid parameters for PUBLISH");
  }
  if (subscriber_ == nullptr) {
    // Stream is already closing.
    return absl::OkStatus();
  }
  QUICHE_RETURN_IF_ERROR(subscriber_->Update(message.parameters));
  CheckStatus(
      SendRequestOk(MessageParameters(), MoqtMessageType::kRequestUpdate));
  return absl::OkStatus();
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtRequestOk& message) {
  if (!message.properties.empty()) {
    OnFatalError(
        absl::InvalidArgumentError("REQUEST_OK received with properties"));
    return absl::OkStatus();
  }
  if (!ParametersAllowedByRequestOk(message.parameters,
                                    MoqtMessageType::kRequestUpdate)) {
    return absl::InvalidArgumentError(
        "REQUEST_OK contains invalid parameters for REQUEST_UPDATE");
  }
  // TODO(martinduke): Process REQUEST_OK parameters.
  return request_update_queue().OnControlMessage(message);
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtRequestError& message) {
  if (!RedirectAllowedByRequestError(message.redirect,
                                     MoqtMessageType::kRequestUpdate)) {
    return absl::InvalidArgumentError(
        "REQUEST_ERROR contains invalid redirect for REQUEST_UPDATE");
  }
  absl::Status status = request_update_queue().OnControlMessage(message);
  if (status.ok()) {
    Fin();
  }
  return status;
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtPublishDone& message) {
  if (subscriber_ == nullptr) {
    // PUBLISH_DONE can be sent before the subscriber rejects the track.
    return absl::OkStatus();
  }
  subscriber_->OnPublishDone(message.stream_count, clock_, alarm_factory_);
  return absl::OkStatus();
}

absl::Status MoqtPublishResponseStream::OnControlMessage(
    const MoqtPublishStateNotify& message) {
  if (subscriber_ == nullptr) {
    return absl::OkStatus();
  }
  return subscriber_->Update(message.parameters);
}

}  // namespace moqt
