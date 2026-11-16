// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_fetch_stream.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_trace_recorder.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"
#include "quiche/common/quiche_status_utils.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

MoqtFetchRequestStream::MoqtFetchRequestStream(
    MoqtFramer* framer, const MoqtControlMessageParser& message_parser,
    uint64_t request_id, const FullTrackName& name, Location start,
    Location end, const MessageParameters& parameters,
    UpstreamFetchTask* absl_nonnull task,
    SessionErrorCallback session_error_callback,
    FetchResponseCallback response_callback,
    RemoveFetchCallback delete_callback)
    : MoqtBidiStreamBase(framer, message_parser,
                         std::move(session_error_callback)),
      ObjectSubscriber(name, request_id, parameters, this),
      start_(start),
      end_(end),
      task_(task),
      response_callback_(std::move(response_callback)),
      remove_callback_(std::move(delete_callback)) {
  // This is a temporary callback until the data stream arrives.
  task_->set_task_destroyed_callback([this]() {
    task_ = nullptr;
    Reset(kResetCodeCancelled);
  });
}

MoqtFetchRequestStream::MoqtFetchRequestStream(
    MoqtFramer* framer, const MoqtControlMessageParser& message_parser,
    uint64_t request_id, const FullTrackName& name, uint64_t joining_request_id,
    uint64_t joining_start, bool relative, MessageParameters parameters,
    UpstreamFetchTask* absl_nonnull task,
    SessionErrorCallback session_error_callback,
    FetchResponseCallback response_callback,
    RemoveFetchCallback delete_callback)
    : MoqtBidiStreamBase(framer, message_parser,
                         std::move(session_error_callback)),
      ObjectSubscriber(name, request_id, parameters, this),
      start_(relative ? Location(0, 0) : Location(joining_start, 0)),
      end_(Location(kMaxGroupId, kMaxObjectId)),
      joining_request_id_(joining_request_id),
      relative_groups_(relative ? std::make_optional(joining_start)
                                : std::nullopt),
      task_(std::move(task)),
      response_callback_(std::move(response_callback)),
      remove_callback_(std::move(delete_callback)) {
  // This is a temporary callback until the data stream arrives.
  task_->set_task_destroyed_callback([this]() {
    task_ = nullptr;
    Reset(kResetCodeCancelled);
  });
}

void MoqtFetchRequestStream::OnStreamOpened(
    webtransport::StreamVisitor* stream) {
  // Remove the stream from the request ID map, because no other stream can be
  // assigned to this request ID.
  if (remove_callback_ == nullptr) {
    QUICHE_BUG(quiche_bug_moqt_fetch_stream_already_detached)
        << "OnStreamOpened called after Detach";
    return;
  }
  RemoveFetchCallback delete_callback = std::move(remove_callback_);
  remove_callback_ = nullptr;
  std::move(delete_callback)(request_id());
  UpstreamFetchTask* task = task_;
  // Interactions with the task will now be mediated through the uni stream.
  // If the uni stream closes, so will the bidi stream.
  task_ = nullptr;
  auto* data_stream = absl::down_cast<IncomingDataStream*>(stream);
  data_stream->set_group_order(
      const_parameters().group_order.value_or(MoqtGroupOrder::kAscending));
  data_stream->set_fetch_task(task);
}

void MoqtFetchRequestStream::OnStreamClosed(absl::Status status,
                                            std::optional<DataStreamIndex>) {
  if (status.ok()) {
    Fin();
  } else {
    Reset(StatusToMoqtStreamError(status));
  }
}

void MoqtFetchRequestStream::OnStreamBound() {
  // TODO(martinduke): Set the priority for this stream.
  MoqtFetch fetch;
  fetch.request_id = request_id();
  fetch.parameters = const_parameters();
  if (!joining_request_id_.has_value()) {
    fetch.fetch = StandaloneFetch(full_track_name(), start_, end_);
  } else if (relative_groups_.has_value()) {
    fetch.fetch = JoiningFetchRelative(*joining_request_id_, *relative_groups_);
  } else {
    fetch.fetch = JoiningFetchAbsolute(*joining_request_id_, start_.group);
  }
  SendOrBufferMessageOrFatal(framer()->SerializeFetch(fetch));
}

absl::Status MoqtFetchRequestStream::OnRawControlMessage(
    const MoqtRawControlMessage& message) {
  return ControlMessageDispatcher::DispatchControlMessage(
      *this, message_parser(), message, "fetch request");
}

absl::Status MoqtFetchRequestStream::OnControlMessage(
    const MoqtFetchOk& message) {
  if (response_callback_ == nullptr) {
    return absl::InvalidArgumentError("Multiple FETCH_OK on the same stream");
  }
  absl::Status mandatory_property_status =
      message.properties.CheckForUnknownMandatoryProperty();
  if (!mandatory_property_status.ok()) {
    FetchResponseCallback response_callback = std::move(response_callback_);
    response_callback_ = nullptr;
    Reset(kResetCodeCancelled);
    std::move(response_callback)(
        StatusToMoqtRequestError(mandatory_property_status));
    return absl::OkStatus();
  }
  QUIC_DLOG(INFO) << "Received the FETCH_OK for " << full_track_name();
  if (relative_groups_.has_value() &&
      (*relative_groups_ < message.end_location.group)) {
    start_ = Location(message.end_location.group - *relative_groups_, 0);
    relative_groups_.reset();
  }
  end_ = std::min(end_, message.end_location);
  FetchResponseCallback response_callback = std::move(response_callback_);
  response_callback_ = nullptr;
  std::move(response_callback)(message);
  return absl::OkStatus();
}

absl::Status MoqtFetchRequestStream::OnControlMessage(
    const MoqtRequestOk& message) {
  if (!message.properties.empty()) {
    OnFatalError(absl::InvalidArgumentError(
        "REQUEST_UPDATE_OK received with properties"));
    return absl::OkStatus();
  }
  if (!ParametersAllowedByRequestOk(message.parameters,
                                    MoqtMessageType::kRequestUpdate)) {
    return absl::InvalidArgumentError(
        "REQUEST_OK contains invalid parameters for REQUEST_UPDATE");
  }
  absl::StatusOr<MessageParameters> old_parameters =
      request_update_queue().NextParameters();
  if (!old_parameters.ok()) {
    return old_parameters.status();
  }
  QUICHE_RETURN_IF_ERROR(Update(*old_parameters));
  QUICHE_RETURN_IF_ERROR(Update(message.parameters));
  return request_update_queue().OnControlMessage(message);
}

absl::Status MoqtFetchRequestStream::OnControlMessage(
    const MoqtRequestError& message) {
  if (response_callback_ != nullptr) {
    if (!RedirectAllowedByRequestError(message.redirect,
                                       MoqtMessageType::kFetch)) {
      return absl::InvalidArgumentError(
          "REQUEST_ERROR contains invalid redirect for FETCH");
    }
    FetchResponseCallback response_callback = std::move(response_callback_);
    response_callback_ = nullptr;
    std::move(response_callback)(message);
    set_status(
        RequestErrorCodeToStatus(message.error_code, message.reason_phrase));
    Fin();
    // Don't do anything to the data stream or the task, except set the status.
    // Let the data stream closure control the task. Note that if the uni stream
    // arrives after REQUEST_ERROR, the record of the request ID will be gone,
    // and IncomingDataStream will therefore send STOP_SENDING immediately.
    return absl::OkStatus();
  }
  // Response to REQUEST_UPDATE.
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

void MoqtFetchRequestStream::Detach() {
  if (task_ != nullptr) {
    // Notify the task (which the application owns) that nothing more is coming.
    // If this has already been called, UpstreamFetchTask will ignore it.
    task_->OnStreamAndFetchClosed(stream_status());
    task_ = nullptr;
  }
  if (remove_callback_ != nullptr) {
    RemoveFetchCallback callback = std::move(remove_callback_);
    remove_callback_ = nullptr;
    std::move(callback)(request_id());
  }
  // The uni stream holds a weakptr to this class, so it doesn't need to be
  // notified.
}

MoqtFetchResponseStream::MoqtFetchResponseStream(
    MoqtFramer* absl_nonnull framer,
    const MoqtControlMessageParser& message_parser,
    MoqtPublisher* absl_nonnull application,
    SessionErrorCallback session_error_callback,
    ValidateRequestIdCallback validate_request_id,
    OpenStreamCallback open_stream_callback,
    GetSubscriptionCallback get_subscription_callback)
    : MoqtBidiStreamBase(framer, message_parser,
                         std::move(session_error_callback)),
      data_stream_(nullptr),
      application_(application),
      validate_request_id_(std::move(validate_request_id)),
      open_stream_callback_(std::move(open_stream_callback)),
      get_subscription_callback_(std::move(get_subscription_callback)),
      weak_ptr_factory_(this) {}

absl::Status MoqtFetchResponseStream::OnRawControlMessage(
    const MoqtRawControlMessage& message) {
  return ControlMessageDispatcher::DispatchControlMessage(
      *this, message_parser(), message, "fetch response");
}

absl::Status MoqtFetchResponseStream::OnControlMessage(
    const MoqtFetch& message) {
  if (request_id_.has_value()) {
    return absl::InvalidArgumentError(
        "FETCH received on stream that already has a fetch");
  }
  QUICHE_RETURN_IF_ERROR(validate_request_id_(message.request_id));
  request_id_ = message.request_id;
  parameters_ = message.parameters;
  MoqtGroupOrder group_order =
      message.parameters.group_order.value_or(MoqtGroupOrder::kAscending);
  std::unique_ptr<MoqtFetchTask> fetch;
  FetchResponseCallback response_callback =
      [weak_ptr = weak_ptr_factory_.Create()](
          std::variant<FetchOkData, MoqtRequestErrorInfo> result) {
        MoqtFetchResponseStream* stream = weak_ptr.GetIfAvailable();
        if (stream == nullptr) {
          return;
        }
        if (std::holds_alternative<FetchOkData>(result)) {
          auto& ok_data = std::get<FetchOkData>(result);
          stream->default_publisher_priority_ =
              ok_data.properties.default_publisher_priority();
          SanitizeParameters(ok_data.parameters, MoqtMessageType::kFetchOk);
          stream->parameters_.Update(ok_data.parameters);
          stream->SendOrBufferMessageOrFatal(
              stream->framer()->SerializeFetchOk(ok_data));
          return;
        }
        const auto& error = std::get<MoqtRequestErrorInfo>(result);
        stream->CheckStatus(stream->SendRequestError(error));
      };
  if (std::holds_alternative<StandaloneFetch>(message.fetch)) {
    const StandaloneFetch& standalone_fetch =
        std::get<StandaloneFetch>(message.fetch);
    FullTrackName track_name = standalone_fetch.full_track_name;
    if (track_name.DoesNotExist()) {
      return SendRequestError(RequestErrorCode::kDoesNotExist,
                              /*retry_interval=*/std::nullopt,
                              "Reserved track name");
    }
    std::shared_ptr<MoqtTrackPublisher> track_publisher =
        application_->GetTrack(track_name);
    if (track_publisher == nullptr) {
      QUIC_DLOG(INFO) << "FETCH for " << track_name
                      << " rejected by the application: not found";
      return SendRequestError(RequestErrorCode::kDoesNotExist, std::nullopt,
                              "not found");
    }
    QUIC_DLOG(INFO) << "Received a StandaloneFETCH for " << track_name;
    fetch = track_publisher->StandaloneFetch(
        standalone_fetch.start_location, standalone_fetch.end_location,
        group_order, std::move(response_callback));
  } else {
    // Joining Fetch.
    // TODO(martinduke): There are some gaps in this implementation that are
    // relevant to draft-18 Joining FETCH but not later drafts, where it occurs
    // on the SUBSCRIBE stream and therefore does not need a reference request
    // ID:
    // 1. draft-18 is ambiguous about whether to use the original request ID or
    // the request ID of a REQUEST_UPDATE. Our current request ID tracking data
    // structures and callbacks are not well-equipped to track multiple request
    // IDs per subscription. There are good reasons to use the REQUEST_UPDATE
    // ID: it can prevent a FETCH from failing because forward is set to zero.
    // However, our draft-18 implementation will fail any joining FETCH that
    // beats such a REQUEST_UPDATE.
    // 2. It's hard to distinguish between a request ID that used to exist and
    // one that has yet to arrive. It would be best to use
    // MoqtSession::next_incoming_request_id to guess if the request is new or
    // old, and buffer the stream if it's new.
    //
    // draft-18 is a candidate for long term support, but it may not be worth it
    // to fix problems that don't exist in future drafts if joining FETCH is
    // not important to early-deployed use cases.
    uint64_t joining_request_id =
        std::holds_alternative<JoiningFetchRelative>(message.fetch)
            ? std::get<JoiningFetchRelative>(message.fetch).joining_request_id
            : std::get<JoiningFetchAbsolute>(message.fetch).joining_request_id;
    if (get_subscription_callback_ == nullptr) {
      QUIC_DLOG(INFO)
          << "Received a JOINING_FETCH without subscription callback";
      return SendRequestError(RequestErrorCode::kInternalError, std::nullopt,
                              "Internal error");
    }
    LivePublisher* subscription =
        std::move(get_subscription_callback_)(joining_request_id);
    get_subscription_callback_ = nullptr;
    if (subscription == nullptr) {
      QUIC_DLOG(INFO) << "Received a JOINING_FETCH for request_id "
                      << joining_request_id << " that does not exist";
      return SendRequestError(RequestErrorCode::kInvalidJoiningRequestId,
                              std::nullopt,
                              "Joining Fetch for non-existent request");
    }
    if (!subscription->can_have_joining_fetch()) {
      QUIC_DLOG(INFO) << "Received a JOINING_FETCH for joining_request_id "
                      << joining_request_id << " that is not forwarding";
      return absl::InvalidArgumentError(
          "Joining Fetch for non-forwarding subscribe");
    }
    if (subscription->largest_object_known()) {
      const std::optional<Location> largest_object =
          subscription->parameters().largest_object;
      if (!largest_object.has_value()) {
        // Nothing to Fetch.
        return SendRequestError(RequestErrorCode::kDoesNotExist, std::nullopt,
                                "not found");
      }
      uint64_t start_group;
      if (std::holds_alternative<JoiningFetchRelative>(message.fetch)) {
        const JoiningFetchRelative& relative_fetch =
            std::get<JoiningFetchRelative>(message.fetch);
        start_group =
            (relative_fetch.joining_start > largest_object->group)
                ? 0
                : (largest_object->group - relative_fetch.joining_start);
      } else {
        const JoiningFetchAbsolute& absolute_fetch =
            std::get<JoiningFetchAbsolute>(message.fetch);
        start_group = absolute_fetch.joining_start;
        if (start_group > largest_object->group) {
          return SendRequestError(RequestErrorCode::kInvalidRange, std::nullopt,
                                  "invalid range");
        }
      }
      fetch = subscription->publisher().StandaloneFetch(
          Location{start_group, 0}, *largest_object, group_order,
          std::move(response_callback));
    } else {
      // Subscription is in PENDING state.
      if (std::holds_alternative<JoiningFetchRelative>(message.fetch)) {
        fetch = subscription->publisher().RelativeFetch(
            std::get<JoiningFetchRelative>(message.fetch).joining_start,
            group_order, std::move(response_callback));
      } else {
        fetch = subscription->publisher().AbsoluteFetch(
            std::get<JoiningFetchAbsolute>(message.fetch).joining_start,
            group_order, std::move(response_callback));
      }
    }
  }
  if (fetch == nullptr || !fetch->GetStatus().ok()) {
    QUIC_DLOG(INFO) << "FETCH could not initialize the task";
    return absl::OkStatus();
  }
  fetch_ = std::move(fetch);
  // Set a temporary new-object callback that creates a data stream. When
  // created, the stream visitor will replace this callback.
  fetch_->SetObjectAvailableCallback([this]() {
    if (open_stream_callback_ != nullptr) {
      OpenStreamCallback callback = std::move(open_stream_callback_);
      open_stream_callback_ = nullptr;
      std::move(callback)(
          stream_id(),
          MoqtTrackPriority{parameters_.subscriber_priority.value_or(
                                kDefaultSubscriberPriority),
                            default_publisher_priority_});
    }
  });
  return absl::OkStatus();
}

absl::Status MoqtFetchResponseStream::OnControlMessage(
    const MoqtRequestUpdate& message) {
  QUICHE_RETURN_IF_ERROR(validate_request_id_(message.request_id));
  if (!ParametersAllowedByRequestUpdate(message.parameters,
                                        MoqtMessageType::kFetch)) {
    return absl::InvalidArgumentError(
        "REQUEST_UPDATE contains invalid parameters for FETCH");
  }
  if (data_stream_ != nullptr &&
      message.parameters.subscriber_priority.has_value()) {
    data_stream_->UpdatePriority(*message.parameters.subscriber_priority);
  }
  return SendRequestOk(MessageParameters(), MoqtMessageType::kRequestUpdate);
}

void MoqtFetchResponseStream::OnDataStreamOpen(
    webtransport::Stream* absl_nonnull stream,
    MoqtTraceRecorder* trace_recorder) {
  if (stream == nullptr) {
    return;
  }
  webtransport::StreamPriority priority = {
      kMoqtSendGroupId,
      SendOrderForFetch(parameters_.subscriber_priority.value_or(
          kDefaultSubscriberPriority))};
  // The line below will lead to updating ObjectsAvailableCallback in the
  // FetchTask to call OnCanWrite() on the stream. If there is an object
  // available, the callback will be invoked synchronously (i.e. before
  // SetVisitor() returns).
  if (!request_id_.has_value()) {
    QUICHE_BUG(moqt_bug_data_stream_without_request_id)
        << "OnDataStreamOpen called with no request ID";
    return;
  }
  auto fetch_stream = std::make_unique<OutgoingFetchStream>(
      *framer(), stream, *request_id_, priority, std::move(fetch_),
      [this](absl::Status status) {
        // If |this| has closed, it will have called Detach() and
        // data_stream_->Reset(), which deletes this callback without invoking
        // it. Therefore, there can't be a use-after-free.
        data_stream_ = nullptr;
        if (status.ok()) {
          Fin();  // Clean teardown of the data stream.
        } else {
          Reset(StatusToMoqtStreamError(status));
        }
      },
      trace_recorder);
  fetch_ = nullptr;
  data_stream_ = fetch_stream.get();
  stream->SetVisitor(std::move(fetch_stream));
  data_stream_->Init();
}

void MoqtFetchResponseStream::Detach() {
  if (data_stream_ != nullptr && !stream_status().ok()) {
    // If the bidi stream FINed, let the data stream close on its own.
    data_stream_->OnBidiStreamReset(StatusToMoqtStreamError(stream_status()));
  }
  data_stream_ = nullptr;
  fetch_ = nullptr;
}

}  // namespace moqt
