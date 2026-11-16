// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_session.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_stream.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_namespace_stream.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_parser.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publish_namespace_stream.h"
#include "quiche/quic/moqt/moqt_publish_stream.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_subscribe_stream.h"
#include "quiche/quic/moqt/moqt_track_status_stream.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_callbacks.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/web_transport/web_transport.h"

#define ENDPOINT \
  (perspective() == Perspective::IS_SERVER ? "MoQT Server: " : "MoQT Client: ")

namespace moqt {

namespace {

using ::quic::Perspective;

class DefaultPublisher : public MoqtPublisher {
 public:
  static DefaultPublisher* GetInstance() {
    static DefaultPublisher* instance = new DefaultPublisher();
    return instance;
  }

  // MoqtPublisher implementation.
  absl_nullable std::shared_ptr<MoqtTrackPublisher> GetTrack(
      const FullTrackName& track_name) override {
    QUICHE_DCHECK(track_name.IsValid());
    return nullptr;
  }
};
}  // namespace

MoqtSession::MoqtSession(webtransport::Session* session,
                         MoqtSessionParameters parameters,
                         std::unique_ptr<quic::QuicAlarmFactory> alarm_factory,
                         MoqtSessionCallbacks callbacks)
    : session_(session),
      parameters_(parameters),
      callbacks_(std::move(callbacks)),
      framer_(parameters.using_webtrans, parameters.perspective),
      publisher_(DefaultPublisher::GetInstance()),
      alarm_factory_(std::move(alarm_factory)),
      weak_ptr_factory_(this),
      weak_ptr_factory_for_publishers_(this),
      liveness_token_(std::make_shared<Empty>()) {
  if (parameters_.using_webtrans) {
    session_->SetOnDraining([this]() {
      QUICHE_DLOG(INFO) << "WebTransport session is draining";
      if (callbacks_.goaway_received_callback == nullptr) {
        return;
      }
      MoqtSessionGoAwayCallback callback =
          std::move(callbacks_.goaway_received_callback);
      callbacks_.goaway_received_callback = nullptr;
      std::move(callback)(absl::string_view(), quic::QuicTimeDelta::Zero());
    });
  }
  if (parameters_.perspective == Perspective::IS_SERVER) {
    next_request_id_ = 1;
  }
  QUICHE_DCHECK(parameters_.moqt_implementation.empty());
  parameters_.moqt_implementation = kImplementationName;
}

void MoqtSession::SendControlMessage(quiche::QuicheBuffer message) {
  OutgoingControlStream* control_stream = GetOutgoingControlStream();
  if (control_stream == nullptr) {
    QUICHE_LOG(DFATAL) << "Trying to send a message on the control stream "
                          "while it does not exist";
    return;
  }
  control_stream->SendOrBufferMessageOrFatal(std::move(message));
}

void MoqtSession::OnSessionReady() {
  QUICHE_DLOG(INFO) << ENDPOINT << "Underlying session ready";
  std::optional<std::string> version = session_->GetNegotiatedSubprotocol();
  if (version != parameters_.version) {
    Error(MoqtError::kVersionNegotiationFailed,
          "MOQT peer chose wrong subprotocol");
    return;
  }
  if (!session_->CanOpenNextOutgoingUnidirectionalStream()) {
    Error(MoqtError::kControlMessageTimeout, "Unable to open a control stream");
    return;
  }
  webtransport::Stream* stream = session_->OpenOutgoingUnidirectionalStream();
  if (stream == nullptr) {
    Error(MoqtError::kInternalError, "Unable to open a control stream");
    return;
  }
  auto control_stream = std::make_unique<OutgoingControlStream>(this, stream);
  outgoing_control_stream_ = control_stream->GetWeakPtr();
  trace_recorder_.RecordControlStreamCreated(stream->GetStreamId());
  stream->SetVisitor(std::move(control_stream));
  MoqtSetup setup;
  parameters_.ToSetupOptions(setup.options);
  SendControlMessage(framer_.SerializeSetup(setup));
  QUIC_DLOG(INFO) << ENDPOINT << "Send SETUP";
}

void MoqtSession::OnSessionClosed(webtransport::SessionErrorCode,
                                  const std::string& error_message) {
  if (!error_.empty()) {
    // Avoid erroring out twice.
    return;
  }
  QUICHE_DLOG(INFO) << ENDPOINT << "Underlying session closed with message: "
                    << error_message;
  error_ = error_message;
  CleanUpState();
  std::move(callbacks_.session_terminated_callback)(error_message);
}

void MoqtSession::OnIncomingBidirectionalStreamAvailable() {
  if (!peer_setup_received_) {
    return;
  }
  while (webtransport::Stream* stream =
             session_->AcceptIncomingBidirectionalStream()) {
    if (sent_goaway_) {
      // Immediately reject new requests with REQUEST_ERROR. If the stream
      // cannot be written, just reset it.
      if (!stream->CanWrite()) {
        stream->SendStopSending(kResetCodeGoingAway);
        stream->ResetWithUserCode(kResetCodeGoingAway);
        continue;
      }
      webtransport::StreamWriteOptions options;
      options.set_send_fin(true);
      std::array write_vector = {
          quiche::QuicheMemSlice(framer_.SerializeRequestError(MoqtRequestError{
              RequestErrorCode::kGoingAway, std::nullopt, ""}))};
      if (!stream->Writev(absl::MakeSpan(write_vector), options).ok()) {
        stream->SendStopSending(kResetCodeGoingAway);
        stream->ResetWithUserCode(kResetCodeGoingAway);
      };
      continue;
    }
    auto bidi_stream = std::make_unique<UnknownBidiStream>(this, stream);
    stream->SetVisitor(std::move(bidi_stream));
    stream->visitor()->OnCanRead();
  }
}

void MoqtSession::OnIncomingUnidirectionalStreamAvailable() {
  while (webtransport::Stream* stream =
             session_->AcceptIncomingUnidirectionalStream()) {
    stream->SetVisitor(std::make_unique<UnknownUniStream>(this, stream));
    stream->visitor()->OnCanRead();
  }
}

void MoqtSession::OnDatagramReceived(absl::string_view datagram) {
  MoqtObject message;
  bool use_default_priority;
  absl::StatusOr<absl::string_view> payload =
      ParseDatagram(datagram, message, use_default_priority);
  if (!payload.ok()) {
    if (absl::IsNotFound(payload.status())) {
      return;  // It's PADDING.
    }
    Error(MoqtError::kProtocolViolation, "Malformed datagram received");
    return;
  }
  QUICHE_DLOG(INFO) << ENDPOINT
                    << "Received OBJECT message in datagram for request_id "
                    << " for track alias " << message.track_alias
                    << " with sequence " << message.group_id << ":"
                    << message.object_id << " priority "
                    << message.publisher_priority << " length "
                    << payload->size();
  LiveSubscriber* track = SubscribeByAlias(message.track_alias);
  if (track == nullptr) {
    return;
  }
  track->OnObjectOrOk();
  if (use_default_priority) {
    message.publisher_priority = track->default_publisher_priority();
  }
  if (!track->InWindow(Location(message.group_id, message.object_id))) {
    // TODO(martinduke): a recent REQUEST_UPDATE could put us here, and it's
    // not an error.
    return;
  }
  QUICHE_CHECK(!track->is_fetch());
  SubscribeVisitor* visitor = track->visitor();
  if (visitor != nullptr) {
    PublishedObjectMetadata metadata;
    metadata.location = Location(message.group_id, message.object_id);
    metadata.subgroup = std::nullopt;
    metadata.properties = message.properties;
    metadata.status = message.object_status;
    metadata.publisher_priority = message.publisher_priority;
    metadata.payload_length = payload->size();
    metadata.arrival_time = callbacks_.clock->Now();
    visitor->OnObjectFragment(track->full_track_name(), metadata, *payload,
                              /*offset=*/0);
  }
}

void MoqtSession::OnCanCreateNewOutgoingBidirectionalStream() {
  while (!pending_bidi_streams_.empty() &&
         session_->CanOpenNextOutgoingBidirectionalStream()) {
    webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
    pending_bidi_streams_.front()->BindStream(stream);
    // TODO(vasilvv): Distinguish between control and and non-control bidi
    // streams in trace_recorder_.
    trace_recorder_.RecordControlStreamCreated(stream->GetStreamId());
    stream->SetVisitor(std::move(pending_bidi_streams_.front()));
    pending_bidi_streams_.pop_front();
    stream->visitor()->OnCanWrite();
  }
}

void MoqtSession::Error(MoqtError code, absl::string_view error) {
  if (!error_.empty() || is_closing_) {
    // Avoid erroring out twice.
    return;
  }
  QUICHE_DLOG(INFO) << ENDPOINT << "MOQT session closed with code: "
                    << static_cast<int>(code) << " and message: " << error;
  error_ = std::string(error);
  session_->CloseSession(static_cast<uint64_t>(code), error);
  std::move(callbacks_.session_terminated_callback)(error);
  CleanUpState();
}

std::unique_ptr<MoqtNamespaceTask> MoqtSession::SubscribeNamespace(
    TrackNamespace& prefix, const MessageParameters& parameters,
    MoqtResponseCallback response_callback) {
  if (received_goaway_ || sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to send SUBSCRIBE_NAMESPACE after GOAWAY";
    return nullptr;
  }
  if (prefix.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to subscribe to a reserved track namespace";
    return nullptr;
  }
  if (!outgoing_subscribe_namespace_.SubscribeNamespace(prefix)) {
    return nullptr;
  }
  std::unique_ptr<MoqtSubscribeNamespaceRequestStream> state =
      std::make_unique<MoqtSubscribeNamespaceRequestStream>(
          &framer_, ControlMessageParser(), next_request_id_,
          [weakptr = GetWeakPtr()](const TrackNamespace& prefix) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->outgoing_subscribe_namespace_.UnsubscribeNamespace(
                  prefix);
            }
          },
          [weakptr = GetWeakPtr()](MoqtError error, absl::string_view reason) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->Error(error, reason);
            }
          },
          std::move(response_callback));
  MoqtSubscribeNamespaceRequestStream* state_ptr = state.get();
  if (session_->CanOpenNextOutgoingBidirectionalStream()) {
    webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
    state->BindStream(stream);
    stream->SetVisitor(std::move(state));
  } else {
    pending_bidi_streams_.push_back(std::move(state));
  }
  MoqtSubscribeNamespace message;
  message.request_id = NextRequestId();
  message.track_namespace_prefix = prefix;
  message.parameters = parameters;
  SanitizeParameters(message.parameters, MoqtMessageType::kSubscribeNamespace);
  state_ptr->SendOrBufferMessageOrFatal(
      framer_.SerializeSubscribeNamespace(message));
  QUIC_DLOG(INFO) << ENDPOINT << "Sent SUBSCRIBE_NAMESPACE message for "
                  << message.track_namespace_prefix;
  return state_ptr->CreateTask(prefix);
}

bool MoqtSession::SubscribeTracks(TrackNamespace& prefix,
                                  const MessageParameters& parameters,
                                  MoqtResponseCallback response_callback) {
  if (prefix.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to subscribe to a reserved track namespace";
    return false;
  }
  return false;
}

void MoqtSession::UnsubscribeTracks(TrackNamespace& prefix) {
  // Do nothing.
}

bool MoqtSession::TrackStatus(const FullTrackName& name,
                              const MessageParameters& parameters,
                              TrackStatusResponseCallback response_callback) {
  QUICHE_DCHECK(name.IsValid());
  if (received_goaway_ || sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send TRACK_STATUS after GOAWAY";
    return false;
  }
  if (name.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to get status of a reserved track name";
    return false;
  }
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  if (stream == nullptr) {
    return false;
  }

  MessageParameters sanitized_parameters = parameters;
  SanitizeParameters(sanitized_parameters, MoqtMessageType::kTrackStatus);
  uint64_t request_id = NextRequestId();
  auto stream_visitor = std::make_unique<MoqtTrackStatusRequestStream>(
      &framer_, ControlMessageParser(), request_id, name, sanitized_parameters,
      [session_weak = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSession* session = MoqtSessionFromWeakPtr(session_weak);
        if (session != nullptr) {
          session->Error(code, reason);
        }
      },
      std::move(response_callback));
  MoqtTrackStatusRequestStream* stream_visitor_ptr = stream_visitor.get();
  stream->SetVisitor(std::move(stream_visitor));
  stream_visitor_ptr->BindStream(stream);
  return true;
}

bool MoqtSession::PublishNamespace(
    const TrackNamespace& track_namespace, const MessageParameters& parameters,
    MoqtResponseCallback response_callback,
    quiche::SingleUseCallback<void()> cancel_callback) {
  if (is_closing_ || publish_namespace_requests_.contains(track_namespace)) {
    return false;
  }
  if (received_goaway_ || sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to send PUBLISH_NAMESPACE after GOAWAY";
    return false;
  }
  if (track_namespace.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to publish a reserved namespace";
    return false;
  }
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  if (stream == nullptr) {
    return false;
  }
  MessageParameters sanitized_parameters = parameters;
  SanitizeParameters(sanitized_parameters, MoqtMessageType::kPublishNamespace);
  auto stream_visitor = std::make_unique<MoqtPublishNamespaceRequestStream>(
      track_namespace, sanitized_parameters, &framer_, ControlMessageParser(),
      NextRequestId(),
      [weakptr = GetWeakPtr(), callback = std::move(cancel_callback)](
          const TrackNamespace& prefix) mutable {
        MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
        if (session == nullptr) {
          return;
        }
        session->publish_namespace_requests_.erase(prefix);
        std::move(callback)();
      },
      [weakptr = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
        if (session == nullptr) {
          return;
        }
        session->Error(code, reason);
      },
      std::move(response_callback));
  MoqtPublishNamespaceRequestStream* stream_ptr = stream_visitor.get();
  publish_namespace_requests_.emplace(track_namespace, stream_ptr);
  stream->SetVisitor(std::move(stream_visitor));
  stream_ptr->BindStream(stream);
  return true;
}

bool MoqtSession::PublishNamespaceUpdate(
    const TrackNamespace& track_namespace, MessageParameters& parameters,
    MoqtResponseCallback response_callback) {
  if (is_closing_) {
    return false;
  }
  auto it = publish_namespace_requests_.find(track_namespace);
  if (it == publish_namespace_requests_.end()) {
    QUICHE_BUG(quic_bug_publish_namespace_update_after_closure)
        << "Tried to send PUBLISH_NAMESPACE_UPDATE for unknown namespace "
        << track_namespace;
    return false;
  }
  it->second->CheckStatus(it->second->SendRequestUpdate(
      NextRequestId(), parameters, std::move(response_callback),
      MoqtMessageType::kPublishNamespace));
  return true;
}

bool MoqtSession::PublishNamespaceDone(const TrackNamespace& track_namespace) {
  if (is_closing_) {
    return false;
  }
  auto it = publish_namespace_requests_.find(track_namespace);
  if (it == publish_namespace_requests_.end()) {
    QUICHE_BUG(quic_bug_publish_namespace_update_after_closure)
        << "Tried to reset PUBLISH_NAMESPACE for unknown namespace "
        << track_namespace;
    return false;
  }
  it->second->Reset(kResetCodeCancelled);
  QUIC_DLOG(INFO) << ENDPOINT << "Revoked PUBLISH_NAMESPACE message for "
                  << track_namespace;
  return true;
}

bool MoqtSession::PublishNamespaceCancel(
    const TrackNamespace& track_namespace,
    webtransport::StreamErrorCode error_code) {
  auto it = publish_namespace_responses_.find(track_namespace);
  if (it == publish_namespace_responses_.end()) {
    QUICHE_BUG(quic_bug_publish_namespace_update_after_closure)
        << "Tried to reset PUBLISH_NAMESPACE for unknown namespace "
        << track_namespace;
    return false;
  }
  it->second->Reset(error_code);
  QUIC_DLOG(INFO) << ENDPOINT << "Signalled disinterest in PUBLISH_NAMESPACE "
                  << " for " << track_namespace;
  return true;
}

bool MoqtSession::Subscribe(const FullTrackName& name,
                            SubscribeVisitor* absl_nonnull visitor,
                            const MessageParameters& parameters) {
  QUICHE_DCHECK(name.IsValid());
  if (subscribe_by_name_.contains(name)) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send SUBSCRIBE for track " << name
                    << " which is already subscribed";
    return false;
  }
  if (received_goaway_ || sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send SUBSCRIBE after GOAWAY";
    return false;
  }
  if (!session_->CanOpenNextOutgoingBidirectionalStream()) {
    return false;  // Do not retry opening a SUBSCRIBE stream.
  }
  if (name.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to subscribe to a reserved track name";
    return false;
  }
  MessageParameters sanitized_parameters = parameters;
  SanitizeParameters(sanitized_parameters, MoqtMessageType::kSubscribe);
  auto stream_visitor = std::make_unique<MoqtSubscribeRequestStream>(
      &framer_, ControlMessageParser(), NextRequestId(),
      [weak_session = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSessionInterface* session = weak_session.GetIfAvailable();
        if (session == nullptr) {
          return;
        }
        session->Error(code, reason);
      },
      name, visitor, sanitized_parameters,
      [weakptr = GetWeakPtr()](LiveSubscriber* track) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
        if (session == nullptr || !track->track_alias().has_value()) {
          return false;
        }
        auto [it, success] = session->subscribe_by_alias_.try_emplace(
            *track->track_alias(), track);
        return success;
      },
      [weakptr = GetWeakPtr()](LiveSubscriber* track) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
        if (session == nullptr) {
          return;
        }
        session->subscribe_by_name_.erase(track->full_track_name());
        if (track->track_alias().has_value()) {
          session->subscribe_by_alias_.erase(*track->track_alias());
        }
      },
      callbacks_.clock, alarm_factory_.get());
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  QUICHE_CHECK(stream != nullptr);
  MoqtSubscribeRequestStream* stream_visitor_ptr = stream_visitor.get();
  stream->SetVisitor(std::move(stream_visitor));
  stream_visitor_ptr->BindStream(stream);
  subscribe_by_name_[name] = stream_visitor_ptr->track();
  if (SupportsObjectAck()) {
    visitor->OnCanAckObjects(
        [weak_this = GetWeakPtr(), name](uint64_t group, uint64_t object,
                                         quic::QuicTimeDelta time_delta) {
          if (MoqtSession* session = MoqtSessionFromWeakPtr(weak_this);
              session != nullptr) {
            session->SendObjectAck(name, group, object, time_delta);
          }
        });
  }
  return true;
}

bool MoqtSession::SubscribeUpdate(const FullTrackName& name,
                                  const MessageParameters& parameters,
                                  MoqtResponseCallback response_callback) {
  QUICHE_DCHECK(name.IsValid());
  auto it = subscribe_by_name_.find(name);
  if (it == subscribe_by_name_.end()) {
    return false;
  }
  MessageParameters update_parameters = parameters;
  if (parameters.new_group_request.has_value() &&
      !it->second->dynamic_groups() && !it->second->ErrorIsAllowed()) {
    // Sending NEW_GROUP_REQUEST if the Track Properties have arrived and do not
    // include DYNAMIC_GROUPS.
    update_parameters.new_group_request.reset();
  }
  return it->second->request_stream()
      ->SendRequestUpdate(NextRequestId(), update_parameters,
                          std::move(response_callback),
                          MoqtMessageType::kSubscribe)
      .ok();
}

bool MoqtSession::PublishUpdate(const FullTrackName& name,
                                const MessageParameters& parameters,
                                MoqtResponseCallback response_callback) {
  if (is_closing_) {
    return true;
  }
  QUICHE_DCHECK(name.IsValid());
  auto it = subscribed_track_names_.find(name);
  if (it == subscribed_track_names_.end()) {
    return true;
  }
  return it->second->PublishUpdate(parameters, std::move(response_callback));
}

void MoqtSession::Unsubscribe(const FullTrackName& name) {
  if (is_closing_) {
    return;
  }
  QUICHE_DCHECK(name.IsValid());
  auto it = subscribe_by_name_.find(name);
  if (it == subscribe_by_name_.end()) {
    return;
  }
  it->second->request_stream()->Reset(kResetCodeCancelled);
}

bool MoqtSession::Publish(
    std::shared_ptr<MoqtTrackPublisher> absl_nonnull publisher,
    const MessageParameters& parameters, const TrackProperties& properties,
    MoqtResponseCallback response_callback) {
  if (received_goaway_ || sent_goaway_) {
    QUICHE_DLOG(INFO) << ENDPOINT << "Tried to send PUBLISH after GOAWAY";
    return false;
  }
  const FullTrackName& name = publisher->GetTrackName();
  QUICHE_DCHECK(name.IsValid());
  if (!session_->CanOpenNextOutgoingBidirectionalStream()) {
    return false;  // Do not retry opening a PUBLISH stream.
  }
  if (name.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to publish a reserved track name";
    return false;
  }
  auto it = subscribed_track_names_.find(name);
  if (it != subscribed_track_names_.end()) {
    if (it->second->largest_object_known()) {
      QUICHE_DLOG(INFO) << ENDPOINT << "Tried to send PUBLISH for track "
                        << name << " which is already published";
      return false;
    }
    it->second->OnSubscribeRejected(
        MoqtRequestErrorInfo{RequestErrorCode::kDuplicateSubscription,
                             std::nullopt, "PUBLISH is coming"});
  }
  auto stream_visitor = std::make_unique<MoqtPublishRequestStream>(
      &framer_, ControlMessageParser(),
      [weak_session = GetWeakPtr()](LivePublisher* publisher) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return;
        }
        session->subscribed_track_names_.erase(
            publisher->publisher().GetTrackName());
        session->published_subscriptions_.erase(publisher->request_id());
      },
      [weak_session = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSessionInterface* session = weak_session.GetIfAvailable();
        if (session == nullptr) {
          return;
        }
        session->Error(code, reason);
      },
      [weak_session = GetWeakPtr()](uint64_t request_id) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return absl::NotFoundError("Session is gone");
        }
        return session->ValidateNewIncomingRequestId(request_id);
      },
      std::move(response_callback));
  MessageParameters sanitized_parameters = parameters;
  SanitizeParameters(sanitized_parameters, MoqtMessageType::kPublish);
  auto publish_state = std::make_unique<LivePublisher>(
      framer_, publisher, stream_visitor.get(), NextRequestId(),
      next_local_track_alias_, sanitized_parameters,
      weak_ptr_factory_for_publishers_.Create(), true);
  LivePublisher* publisher_ptr = publish_state.get();
  stream_visitor->SetPublisher(std::move(publish_state));
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  MoqtPublishRequestStream* stream_visitor_ptr = stream_visitor.get();
  stream->SetVisitor(std::move(stream_visitor));
  stream_visitor_ptr->BindStream(stream);
  subscribed_track_names_.emplace(name, publisher_ptr);
  published_subscriptions_.emplace(publisher_ptr->request_id(), publisher_ptr);
  ++next_local_track_alias_;
  publisher->AddObjectListener(publisher_ptr, sanitized_parameters);
  return true;
}

std::unique_ptr<MoqtFetchTask> MoqtSession::Fetch(
    const FullTrackName& name, FetchResponseCallback callback, Location start,
    uint64_t end_group, std::optional<uint64_t> end_object,
    const MessageParameters& parameters) {
  QUICHE_DCHECK(name.IsValid());
  if (received_goaway_ || sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send FETCH after GOAWAY";
    return nullptr;
  }
  if (name.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to fetch a reserved track name";
    return nullptr;
  }
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  if (stream == nullptr) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send FETCH but no more streams";
    return nullptr;
  }
  MessageParameters sanitized_parameters = parameters;
  SanitizeParameters(sanitized_parameters, MoqtMessageType::kFetch);
  uint64_t request_id = NextRequestId();
  auto task = std::make_unique<UpstreamFetchTask>();
  auto fetch = std::make_unique<MoqtFetchRequestStream>(
      &framer_, ControlMessageParser(), request_id, name, start,
      Location(end_group, end_object.value_or(kMaxObjectId)),
      sanitized_parameters, task.get(),
      [weak_session = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return;
        }
        session->Error(code, reason);
      },
      std::move(callback),
      [weak_session = GetWeakPtr()](uint64_t request_id) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return;
        }
        session->fetch_by_id_.erase(request_id);
      });
  MoqtFetchRequestStream* fetch_visitor = fetch.get();
  fetch_by_id_.emplace(request_id, fetch_visitor);
  stream->SetVisitor(std::move(fetch));
  fetch_visitor->BindStream(stream);
  return task;
}

bool MoqtSession::RelativeJoiningFetch(const FullTrackName& name,
                                       SubscribeVisitor* visitor,
                                       uint64_t num_previous_groups,
                                       const MessageParameters& parameters) {
  QUICHE_DCHECK(name.IsValid());
  if (name.DoesNotExist()) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to fetch a reserved track name";
    return false;
  }
  std::unique_ptr<MoqtFetchTask> fetch_task = RelativeJoiningFetch(
      name, visitor, [](std::variant<FetchOkData, MoqtRequestErrorInfo>) {},
      num_previous_groups, parameters);
  if (fetch_task == nullptr) {
    return false;
  }
  LiveSubscriber* subscribe = SubscribeByName(name);
  if (subscribe == nullptr || subscribe->is_fetch()) {
    // fetch_task will be released on exit.
    return false;
  }
  subscribe->OnJoiningFetchReady(std::move(fetch_task));
  return true;
}

std::unique_ptr<MoqtFetchTask> MoqtSession::RelativeJoiningFetch(
    const FullTrackName& name, SubscribeVisitor* visitor,
    FetchResponseCallback callback, uint64_t num_previous_groups,
    const MessageParameters& parameters) {
  QUICHE_DCHECK(name.IsValid());
  MessageParameters subscribe_parameters = parameters;
  subscribe_parameters.location_filter = LocationFilter::NextObjectFilter();
  uint64_t subscribe_request_id = next_request_id_;
  if (!Subscribe(name, visitor, subscribe_parameters)) {
    return nullptr;
  }
  webtransport::Stream* stream = session_->OpenOutgoingBidirectionalStream();
  if (stream == nullptr) {
    // TODO(martinduke): This is a spot where the bool return value is not all
    // that helpful, but the problem will go away when the whole transaction
    // occurs on one stream.
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Tried to send JOINING FETCH but no more "
                       "streams";
    return nullptr;
  }
  QUIC_DLOG(INFO) << ENDPOINT << "Sent Joining FETCH message for " << name;
  MessageParameters fetch_parameters = parameters;
  SanitizeParameters(fetch_parameters, MoqtMessageType::kFetch);
  uint64_t request_id = NextRequestId();
  auto task = std::make_unique<UpstreamFetchTask>();
  auto fetch = std::make_unique<MoqtFetchRequestStream>(
      &framer_, ControlMessageParser(), request_id, name, subscribe_request_id,
      num_previous_groups, /*relative=*/true, fetch_parameters, task.get(),
      [weak_session = GetWeakPtr()](MoqtError code, absl::string_view reason) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return;
        }
        session->Error(code, reason);
      },
      std::move(callback),
      [weak_session = GetWeakPtr()](uint64_t request_id) {
        MoqtSession* session = MoqtSessionFromWeakPtr(weak_session);
        if (session == nullptr) {
          return;
        }
        session->fetch_by_id_.erase(request_id);
      });
  MoqtFetchRequestStream* fetch_visitor = fetch.get();
  fetch_by_id_.emplace(request_id, fetch_visitor);
  stream->SetVisitor(std::move(fetch));
  fetch_visitor->BindStream(stream);
  return task;
}

void MoqtSession::GoAway(absl::string_view new_session_uri) {
  if (sent_goaway_) {
    QUIC_DLOG(INFO) << ENDPOINT << "Tried to send multiple GOAWAY";
    return;
  }
  if (!new_session_uri.empty() &&
      parameters_.perspective == Perspective::IS_CLIENT) {
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Client tried to send GOAWAY with new session URI";
    return;
  }
  MoqtGoAway message;
  message.new_session_uri = std::string(new_session_uri);
  message.timeout = kDefaultGoAwayTimeout;
  SendControlMessage(framer_.SerializeGoAway(message));
  sent_goaway_ = true;
  goaway_timeout_alarm_ = absl::WrapUnique(
      alarm_factory_->CreateAlarm(new GoAwayTimeoutDelegate(this)));
  goaway_timeout_alarm_->Set(callbacks_.clock->ApproximateNow() +
                             kDefaultGoAwayTimeout);
}

void MoqtSession::GoAwayTimeoutDelegate::OnAlarm() {
  session_->Error(MoqtError::kGoawayTimeout,
                  "Peer did not close session after GOAWAY");
}

void MoqtSession::UpdateTrackPriority(
    const FullTrackName& name, std::optional<MoqtTrackPriority> old_priority,
    MoqtTrackPriority new_priority) {
  if (old_priority.has_value()) {
    auto [start, end] =
        requests_with_queued_streams_.equal_range(*old_priority);
    for (auto it = start; it != end; ++it) {
      if (std::holds_alternative<FullTrackName>(it->second) &&
          std::get<FullTrackName>(it->second) == name) {
        requests_with_queued_streams_.erase(it);
        break;
      }
    }
  }
  requests_with_queued_streams_.emplace(new_priority, name);
}

void MoqtSession::UpdateTrackPriority(
    webtransport::StreamId stream_id,
    std::optional<MoqtTrackPriority> old_priority,
    MoqtTrackPriority new_priority) {
  if (old_priority.has_value()) {
    auto [start, end] =
        requests_with_queued_streams_.equal_range(*old_priority);
    for (auto it = start; it != end; ++it) {
      if (std::holds_alternative<webtransport::StreamId>(it->second) &&
          std::get<webtransport::StreamId>(it->second) == stream_id) {
        requests_with_queued_streams_.erase(it);
        break;
      }
    }
  }
  requests_with_queued_streams_.emplace(new_priority, stream_id);
}

std::shared_ptr<MoqtTrackPublisher> MoqtSession::GetTrackPublisher(
    const FullTrackName& name) {
  if (publisher_ == nullptr) {
    return nullptr;
  }
  return publisher_->GetTrack(name);
}

MoqtPublishingMonitorInterface* MoqtSession::ReleaseMonitoringInterface(
    const FullTrackName& name) {
  auto it = monitoring_interfaces_for_published_tracks_.find(name);
  if (it == monitoring_interfaces_for_published_tracks_.end()) {
    return nullptr;
  }
  MoqtPublishingMonitorInterface* interface = it->second;
  monitoring_interfaces_for_published_tracks_.erase(it);
  return interface;
}

LiveSubscriber* MoqtSession::SubscribeByAlias(uint64_t track_alias) {
  auto it = subscribe_by_alias_.find(track_alias);
  if (it == subscribe_by_alias_.end()) {
    return nullptr;
  }
  return it->second;
}

LiveSubscriber* MoqtSession::SubscribeByName(const FullTrackName& track_name) {
  auto it = subscribe_by_name_.find(track_name);
  if (it == subscribe_by_name_.end()) {
    return nullptr;
  }
  return it->second;
}

MoqtFetchRequestStream* MoqtSession::FetchById(uint64_t request_id) {
  auto it = fetch_by_id_.find(request_id);
  if (it == fetch_by_id_.end()) {
    return nullptr;
  }
  return it->second;
}

void MoqtSession::OnCanCreateNewOutgoingUnidirectionalStream() {
  while (!requests_with_queued_streams_.empty() &&
         session_->CanOpenNextOutgoingUnidirectionalStream()) {
    auto next = requests_with_queued_streams_.begin();
    if (std::holds_alternative<FullTrackName>(next->second)) {
      auto it =
          subscribed_track_names_.find(std::get<FullTrackName>(next->second));
      requests_with_queued_streams_.erase(next);
      if (it != subscribed_track_names_.end()) {
        it->second->OnCanCreateNewUniStream();
      }
      continue;
    }
    // FETCH.
    webtransport::StreamId stream_id =
        std::get<webtransport::StreamId>(next->second);
    requests_with_queued_streams_.erase(next);
    webtransport::Stream* stream = session_->GetStreamById(stream_id);
    if (stream == nullptr) {
      // The request is gone, so remove it from the queue and continue.
      continue;
    }
    auto fetch = absl::down_cast<MoqtFetchResponseStream*>(stream->visitor());
    if (fetch == nullptr) {
      QUICHE_BUG(queued_uni_stream_invalid_request_type)
          << "Unknown stream type for request " << stream_id;
      continue;
    }
    fetch->OnDataStreamOpen(session_->OpenOutgoingUnidirectionalStream(),
                            &trace_recorder_);
  }
}

absl::Status MoqtSession::ValidateNewIncomingRequestId(uint64_t request_id) {
  if ((request_id % 2 == 0) !=
      (parameters_.perspective == Perspective::IS_SERVER)) {
    QUIC_DLOG(INFO) << parameters_.perspective
                    << "Request ID evenness incorrect";
    Error(MoqtError::kInvalidRequestId, "Request ID evenness incorrect");
    return absl::InvalidArgumentError("Request ID evenness incorrect");
  }
  // Check the one request ID map we have for the value. Most active request IDs
  // are not tracked at the session level.
  if (published_subscriptions_.contains(request_id)) {
    QUIC_DLOG(INFO) << parameters_.perspective << "Duplicate Request ID";
    Error(MoqtError::kInvalidRequestId, "Duplicate Request ID");
    return absl::InvalidArgumentError("Duplicate Request ID");
  }
  return absl::OkStatus();
}

void MoqtSession::UnknownBidiStream::OnCanRead() {
  absl::StatusOr<uint64_t> type = parser_.ReadStreamType();
  if (absl::IsUnavailable(type.status())) {
    return;
  }
  if (absl::IsInvalidArgument(type.status())) {
    // Received a FIN before any type has been available, which is malformed.
    session_->Error(MoqtError::kProtocolViolation, type.status().message());
    return;
  }
  if (!type.ok()) {
    // The result is neither of "OK", "no type available", or "parse error".
    // This is unexpected; treat it as an internal error, and reset the stream.
    stream_->SendStopSending(kResetCodeInternalError);
    stream_->ResetWithUserCode(kResetCodeInternalError);
    return;
  }
  MoqtMessageType message_type = static_cast<MoqtMessageType>(*type);
  switch (message_type) {
    case MoqtMessageType::kSubscribeNamespace: {
      auto namespace_stream =
          std::make_unique<MoqtSubscribeNamespaceResponseStream>(
              &session_->framer_, session_->ControlMessageParser(),
              [weakptr = session_->GetWeakPtr()](const TrackNamespace& prefix) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  return session->incoming_subscribe_namespace_
                      .SubscribeNamespace(prefix);
                }
                return true;
              },
              [weakptr = session_->GetWeakPtr()](const TrackNamespace& prefix) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  session->incoming_subscribe_namespace_.UnsubscribeNamespace(
                      prefix);
                }
              },
              [weakptr = session_->GetWeakPtr()](MoqtError code,
                                                 absl::string_view reason) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  session->Error(code, reason);
                }
              },
              [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session == nullptr) {
                  return absl::NotFoundError("Session is gone");
                }
                return session->ValidateNewIncomingRequestId(request_id);
              },
              session_->callbacks_.incoming_subscribe_namespace_callback);
      namespace_stream->BindStream(std::move(parser_));
      MoqtSubscribeNamespaceResponseStream* temp_stream =
          namespace_stream.get();
      stream_->SetVisitor(std::move(namespace_stream));
      // The UnknownBidiStream object is deleted; no class access after this
      // point.
      temp_stream->OnCanRead();
      break;
    }
    case MoqtMessageType::kSubscribeTracks: {
      // TODO(martinduke): Implement this.
      MoqtControlMessageQueue queue(stream_);
      if (!queue
               .SendOrBufferMessage(
                   session_->framer_.SerializeRequestError(MoqtRequestError{
                       RequestErrorCode::kNotSupported, std::nullopt,
                       "SUBSCRIBE_TRACKS is not supported"}))
               .ok()) {
        session_->Error(MoqtError::kInternalError, "Internal write error");
        return;
      }
      break;
    }
    case MoqtMessageType::kPublishNamespace: {
      auto publish_namespace_stream =
          std::make_unique<MoqtPublishNamespaceResponseStream>(
              &session_->framer_, session_->ControlMessageParser(),
              [weakptr = session_->GetWeakPtr()](const TrackNamespace& prefix,
                                                 MoqtBidiStreamBase* stream) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session == nullptr) {
                  return false;
                }
                auto [it, success] =
                    session->publish_namespace_responses_.try_emplace(prefix,
                                                                      stream);
                return success;
              },
              [weakptr = session_->GetWeakPtr()](const TrackNamespace& prefix) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  session->publish_namespace_responses_.erase(prefix);
                }
              },
              [weakptr = session_->GetWeakPtr()](MoqtError code,
                                                 absl::string_view reason) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  session->Error(code, reason);
                }
              },
              [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session == nullptr) {
                  return absl::NotFoundError("Session is gone");
                }
                return session->ValidateNewIncomingRequestId(request_id);
              },
              [weakptr = session_->GetWeakPtr()](
                  const TrackNamespace& track_namespace,
                  const MessageParameters* absl_nullable parameters,
                  MoqtResponseCallback callback) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session == nullptr) {
                  return;
                }
                session->callbacks_.incoming_publish_namespace_callback(
                    track_namespace, parameters, std::move(callback));
              });
      publish_namespace_stream->BindStream(std::move(parser_));
      MoqtPublishNamespaceResponseStream* temp_stream =
          publish_namespace_stream.get();
      stream_->SetVisitor(std::move(publish_namespace_stream));
      // The UnknownBidiStream object is deleted; no class access after this
      // point.
      temp_stream->OnCanRead();
      break;
    }
    case MoqtMessageType::kPublish: {
      auto publish_stream = std::make_unique<MoqtPublishResponseStream>(
          &session_->framer_, session_->ControlMessageParser(),
          session_->callbacks_.clock, session_->alarm_factory(),
          [weakptr = session_->GetWeakPtr()](MoqtError code,
                                             absl::string_view reason) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->Error(code, reason);
            }
          },
          [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return absl::NotFoundError("Session is gone");
            }
            return session->ValidateNewIncomingRequestId(request_id);
          },
          &session_->callbacks_.incoming_publish_callback,
          [weakptr = session_->GetWeakPtr()](LiveSubscriber* track) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return false;
            }
            QUICHE_BUG_IF(quiche_bug_publish_no_track_alias,
                          !track->track_alias().has_value())
                << "PUBLISH with no track alias";
            if (!track->track_alias().has_value()) {
              return false;
            }
            auto [alias_it, alias_inserted] =
                session->subscribe_by_alias_.try_emplace(*track->track_alias(),
                                                         track);
            if (!alias_inserted) {
              // Already a PUBLISH or an established SUBSCRIBE.
              return false;
            }
            auto it =
                session->subscribe_by_name_.find(track->full_track_name());
            if (it != session->subscribe_by_name_.end()) {
              // It's a pending SUBSCRIBE; kill it, but use the parameters and
              // visitor from the SUBSCRIBE.
              if (!track->Update(it->second->const_parameters()).ok()) {
                session->Error(MoqtError::kInternalError,
                               "Failed to update parameters");
                return false;
              }
              track->set_visitor(it->second->ReleaseVisitor());
              session->Unsubscribe(it->second->full_track_name());
            }
            auto [name_it, name_inserted] =
                session->subscribe_by_name_.try_emplace(
                    track->full_track_name(), track);
            QUICHE_DCHECK(name_inserted);
            return true;
          },
          [weakptr = session_->GetWeakPtr()](LiveSubscriber* track) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->subscribe_by_name_.erase(track->full_track_name());
              if (track->track_alias().has_value()) {
                session->subscribe_by_alias_.erase(*track->track_alias());
              }
            }
          });
      publish_stream->BindStream(std::move(parser_));
      MoqtPublishResponseStream* temp_stream = publish_stream.get();
      stream_->SetVisitor(std::move(publish_stream));
      // The UnknownBidiStream object is deleted; no class access after this
      // point.
      temp_stream->OnCanRead();
      break;
    }
    case MoqtMessageType::kSubscribe: {
      auto subscribe_stream = std::make_unique<MoqtSubscribeResponseStream>(
          &session_->framer_, session_->ControlMessageParser(),
          session_->next_local_track_alias_++,
          [weakptr = session_->GetWeakPtr()](LivePublisher* subscription) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return true;
            }
            auto [it, success] = session->published_subscriptions_.try_emplace(
                subscription->request_id(), subscription);
            if (!success) {
              return false;
            }
            auto [it2, success2] = session->subscribed_track_names_.try_emplace(
                subscription->publisher().GetTrackName(), subscription);
            return success2;
          },
          [weakptr = session_->GetWeakPtr()](LivePublisher* subscription) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return;
            }
            session->published_subscriptions_.erase(subscription->request_id());
            session->subscribed_track_names_.erase(
                subscription->publisher().GetTrackName());
          },
          [weakptr = session_->GetWeakPtr()](MoqtError code,
                                             absl::string_view reason) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->Error(code, reason);
            }
          },
          [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return absl::NotFoundError("Session is gone");
            }
            return session->ValidateNewIncomingRequestId(request_id);
          },
          session_->weak_ptr_factory_for_publishers_.Create());
      subscribe_stream->BindStream(std::move(parser_));
      MoqtSubscribeResponseStream* temp_stream = subscribe_stream.get();
      stream_->SetVisitor(std::move(subscribe_stream));
      // The UnknownBidiStream object is deleted; no class access after this
      // point.
      temp_stream->OnCanRead();
      break;
    }
    case MoqtMessageType::kTrackStatus: {
      auto track_status_stream =
          std::make_unique<MoqtTrackStatusResponseStream>(
              &session_->framer_, session_->ControlMessageParser(),
              [weakptr = session_->GetWeakPtr()](MoqtError code,
                                                 absl::string_view reason) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session != nullptr) {
                  session->Error(code, reason);
                }
              },
              [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
                MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
                if (session == nullptr) {
                  return absl::NotFoundError("Session is gone");
                }
                return session->ValidateNewIncomingRequestId(request_id);
              },
              session_->weak_ptr_factory_for_publishers_.Create());
      track_status_stream->BindStream(std::move(parser_));
      MoqtTrackStatusResponseStream* temp_stream = track_status_stream.get();
      stream_->SetVisitor(std::move(track_status_stream));
      temp_stream->OnCanRead();
      break;
    }
    case MoqtMessageType::kFetch: {
      auto fetch_stream = std::make_unique<MoqtFetchResponseStream>(
          &session_->framer_, session_->ControlMessageParser(),
          session_->publisher_,
          [weakptr = session_->GetWeakPtr()](MoqtError code,
                                             absl::string_view reason) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session != nullptr) {
              session->Error(code, reason);
            }
          },
          [weakptr = session_->GetWeakPtr()](uint64_t request_id) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return absl::NotFoundError("Session is gone");
            }
            return session->ValidateNewIncomingRequestId(request_id);
          },
          // OpenStreamCallback
          [weakptr = session_->GetWeakPtr()](webtransport::StreamId stream_id,
                                             MoqtTrackPriority priority) {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return;
            }
            if (!session->session_->CanOpenNextOutgoingUnidirectionalStream()) {
              session->UpdateTrackPriority(stream_id, std::nullopt, priority);
              return;
            }
            webtransport::Stream* wt_stream =
                session->session_->GetStreamById(stream_id);
            if (wt_stream == nullptr) {
              QUICHE_BUG(
                  quiche_bug_OpenStreamCallback_called_by_nonexistent_stream)
                  << "OpenStreamCallback called by non-existent stream "
                  << stream_id;
              return;
            }
            MoqtFetchResponseStream* response_stream =
                absl::down_cast<MoqtFetchResponseStream*>(wt_stream->visitor());
            if (response_stream == nullptr) {
              QUICHE_BUG(quiche_bug_fetch_response_stream_not_found)
                  << "Failed to get fetch response stream for id " << stream_id;
              return;
            }
            response_stream->OnDataStreamOpen(
                session->session_->OpenOutgoingUnidirectionalStream(),
                &session->trace_recorder());
          },
          // GetSubscriptionCallback
          [weakptr =
               session_->GetWeakPtr()](uint64_t request_id) -> LivePublisher* {
            MoqtSession* session = MoqtSessionFromWeakPtr(weakptr);
            if (session == nullptr) {
              return nullptr;
            }
            auto it = session->published_subscriptions_.find(request_id);
            if (it == session->published_subscriptions_.end()) {
              return nullptr;
            }
            return it->second;
          });
      fetch_stream->BindStream(std::move(parser_));
      MoqtFetchResponseStream* temp_stream = fetch_stream.get();
      stream_->SetVisitor(std::move(fetch_stream));
      temp_stream->OnCanRead();
      break;
    }
    default:
      session_->Error(MoqtError::kProtocolViolation,
                      "Unexpected message type received to start bidi stream");
      return;
  }
}

void MoqtSession::UnknownUniStream::OnCanRead() {
  MoqtSession* session = MoqtSessionFromWeakPtr(session_);
  if (session == nullptr || session->is_closing_) {
    return;
  }
  absl::StatusOr<uint64_t> type = parser_.ReadStreamType();
  if (absl::IsUnavailable(type.status())) {
    return;
  }
  if (absl::IsInvalidArgument(type.status())) {
    // Received a FIN before any type has been available, which is malformed.
    session->Error(MoqtError::kProtocolViolation, type.status().message());
    return;
  }
  if (!type.ok()) {
    stream_->SendStopSending(kResetCodeInternalError);
    return;
  }
  if (*type == static_cast<uint64_t>(MoqtMessageType::kSetup)) {
    if (session->incoming_control_stream_.GetIfAvailable() != nullptr) {
      session->Error(MoqtError::kProtocolViolation, "Multiple control streams");
      return;
    }
    session->trace_recorder().RecordControlStreamCreated(
        stream_->GetStreamId());
    auto control_stream =
        std::make_unique<IncomingControlStream>(session, std::move(parser_));
    IncomingControlStream* temp_stream = control_stream.get();
    session->incoming_control_stream_ = temp_stream->GetWeakPtr();
    // The line below destroys `this`.
    stream_->SetVisitor(std::move(control_stream));
    temp_stream->OnCanRead();
    return;
  }
  if (*type == kPaddingStreamType) {
    auto padding_stream = std::make_unique<IncomingPaddingStream>(stream_);
    IncomingPaddingStream* temp_stream = padding_stream.get();
    // The line below destroys `this`.
    stream_->SetVisitor(std::move(padding_stream));
    temp_stream->OnCanRead();
    return;
  }
  if (!MoqtDataStreamType::FromValue(*type).has_value()) {
    session->Error(MoqtError::kProtocolViolation, "Unknown stream type");
    return;
  }
  auto data_stream = std::make_unique<IncomingDataStream>(
      std::move(parser_), session, session->callbacks_.clock);
  IncomingDataStream* temp_stream = data_stream.get();
  // The line below destroys `this`.
  stream_->SetVisitor(std::move(data_stream));
  temp_stream->OnCanRead();
}

MoqtSession::IncomingControlStream::IncomingControlStream(
    MoqtSession* absl_nonnull session, MoqtStreamTypeParser type_parser)
    : session_(session->GetWeakPtr()),
      parser_(std::move(type_parser)),
      weak_ptr_factory_(this) {}

void MoqtSession::IncomingControlStream::OnCanRead() {
  quiche::QuicheWeakPtr<IncomingControlStream> weak_this =
      weak_ptr_factory_.Create();
  MoqtSession* session = MoqtSessionFromWeakPtr(session_);
  if (session == nullptr || session->is_closing_) {
    return;
  }
  while (true) {
    absl::StatusOr<MoqtRawControlMessage> message = parser_.ReadNextMessage();
    if (absl::IsUnavailable(message.status())) {
      return;
    }
    if (!message.ok()) {
      std::optional<MoqtError> error_code =
          GetMoqtErrorForStatus(message.status());
      session->Error(error_code.value_or(MoqtError::kProtocolViolation),
                     message.status().message());
      return;
    }

    absl::Status status = ControlMessageDispatcher::DispatchControlMessage(
        *session, session->ControlMessageParser(), *message, "control");
    // `DispatchControlMessage` might have closed the session by itself,
    // resulting in the stream and/or the session object being deleted.
    if (!weak_this.IsValid() || !session_.IsValid() || session->is_closing_) {
      return;
    }
    if (!status.ok()) {
      std::optional<MoqtError> error_code = GetMoqtErrorForStatus(status);
      session->Error(error_code.value_or(MoqtError::kProtocolViolation),
                     status.message());
      return;
    }
  }
}

void MoqtSession::IncomingControlStream::OnResetStreamReceived(
    webtransport::StreamErrorCode /*error*/) {
  MoqtSession* session = MoqtSessionFromWeakPtr(session_);
  if (session != nullptr) {
    session->Error(MoqtError::kProtocolViolation,
                   "Control stream reset received");
  }
}

MoqtSession::OutgoingControlStream::OutgoingControlStream(
    MoqtSession* absl_nonnull session,
    webtransport::Stream* absl_nonnull stream)
    : session_(session->GetWeakPtr()),
      outgoing_message_queue_(stream),
      weak_ptr_factory_(this) {
  if (stream != nullptr) {
    stream->SetPriority(webtransport::StreamPriority{
        /*send_group_id=*/kMoqtSendGroupId,
        /*send_order=*/kMoqtControlStreamSendOrder});
  }
}

void MoqtSession::OutgoingControlStream::OnCanWrite() {
  CheckStatus(outgoing_message_queue_.OnCanWrite());
}

void MoqtSession::OutgoingControlStream::OnStopSendingReceived(
    webtransport::StreamErrorCode /*error*/) {
  MoqtSession* session = MoqtSessionFromWeakPtr(session_);
  if (session != nullptr) {
    session->Error(MoqtError::kProtocolViolation,
                   "Control stream stop sending received");
  }
}

void MoqtSession::OutgoingControlStream::CheckStatus(absl::Status status) {
  MoqtSession* session = MoqtSessionFromWeakPtr(session_);
  if (session == nullptr) {
    return;
  }
  if (!status.ok() && !session->is_closing_) {
    std::optional<MoqtError> error_code = GetMoqtErrorForStatus(status);
    session->Error(error_code.value_or(MoqtError::kInternalError),
                   status.message());
  }
}

absl::Status MoqtSession::OnControlMessage(const MoqtSetup& message) {
  if (peer_setup_received_) {
    return absl::InvalidArgumentError("Duplicate SETUP message");
  }
  peer_setup_received_ = true;
  peer_supports_object_ack_ =
      message.options.support_object_acks.value_or(kDefaultSupportObjectAcks);
  QUIC_DLOG(INFO) << ENDPOINT << "Received the SETUP message";
  // TODO: handle path.
  if (callbacks_.session_established_callback != nullptr) {
    MoqtSessionEstablishedCallback callback =
        std::move(callbacks_.session_established_callback);
    callbacks_.session_established_callback = nullptr;
    std::move(callback)();
  }
  // Drain streams that were potentially stalled due to a missing SETUP.
  OnIncomingBidirectionalStreamAvailable();
  return absl::OkStatus();
}

absl::Status MoqtSession::OnControlMessage(const MoqtGoAway& message) {
  if (received_goaway_) {
    return absl::InvalidArgumentError(
        "Received multiple GOAWAY on control stream");
  }
  received_goaway_ = true;
  if (callbacks_.goaway_received_callback == nullptr) {
    // An HTTP/3 or WebTransport GOAWAY already invoked the callback.
    return absl::OkStatus();
  }
  MoqtSessionGoAwayCallback callback =
      std::move(callbacks_.goaway_received_callback);
  callbacks_.goaway_received_callback = nullptr;
  std::move(callback)(message.new_session_uri, message.timeout);
  return absl::OkStatus();
}

void MoqtSession::OnMalformedTrack(ObjectSubscriber* track) {
  if (track->is_fetch()) {
    QUICHE_BUG(quiche_bug_malformed_fetch_track)
        << "Malformed FETCH track should be handled in the data stream";
    return;
  }
  auto* subscribe = absl::down_cast<LiveSubscriber*>(track);
  if (subscribe->visitor() != nullptr) {
    subscribe->visitor()->OnMalformedTrack(track->full_track_name());
  }
  subscribe->request_stream()->Reset(kResetCodeMalformedTrack);
}

void MoqtSession::CleanUpState() {
  if (is_closing_) {
    return;
  }
  is_closing_ = true;
  if (goaway_timeout_alarm_ != nullptr) {
    goaway_timeout_alarm_->PermanentCancel();
  }
  // Although PUBLISH_NAMESPACE state will be cleaned up/ by the owning stream,
  // the session can be destroyed first. In this case, the application callbacks
  // will be inaccessible. Instead, invoke application callbacks now.
  while (!publish_namespace_responses_.empty()) {
    auto it = publish_namespace_responses_.begin();
    MoqtBidiStreamBase* stream = it->second;
    publish_namespace_responses_.erase(it);
    stream->Detach();
  }
  while (!publish_namespace_requests_.empty()) {
    auto it = publish_namespace_requests_.begin();
    MoqtBidiStreamBase* stream = it->second;
    publish_namespace_requests_.erase(it);
    stream->Detach();
  }
  for (auto& [track_name, subscriber] : subscribe_by_name_) {
    // It's possible the application is going to destroy its visitor as early
    // as session_deleted_callback is called. So call OnPublishDone() now and
    // clear the visitor.
    if (subscriber->visitor() != nullptr) {
      subscriber->visitor()->OnPublishDone(subscriber->full_track_name());
      subscriber->ReleaseVisitor();
    }
  }
}

void MoqtSessionParameters::ToSetupOptions(SetupOptions& out) const {
  if (perspective == quic::Perspective::IS_CLIENT && !using_webtrans) {
    out.path = path;
    out.authority = authority;
  }
  if (max_auth_token_cache_size != kDefaultMaxAuthTokenCacheSize) {
    out.max_auth_token_cache_size = max_auth_token_cache_size;
  }
  if (support_object_acks != kDefaultSupportObjectAcks) {
    out.support_object_acks = support_object_acks;
  }
  if (!moqt_implementation.empty()) {
    out.moqt_implementation = moqt_implementation;
  }
  for (const AuthToken& token : authorization_token) {
    out.authorization_tokens.push_back(token);
  }
}

}  // namespace moqt
