// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_live_publisher.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_stream_map.h"
#include "quiche/quic/moqt/moqt_trace_recorder.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

LivePublisher::LivePublisher(
    MoqtFramer framer, std::shared_ptr<MoqtTrackPublisher> track_publisher,
    MoqtBidiStreamBase* absl_nonnull bidi_stream, uint64_t request_id,
    uint64_t track_alias, const MessageParameters& parameters,
    quiche::QuicheWeakPtr<SessionToPublisherInterface> visitor, bool is_publish)
    : track_publisher_(track_publisher),
      bidi_stream_(bidi_stream),
      visitor_(visitor),
      request_id_(request_id),
      largest_object_known_(is_publish),
      is_publish_(is_publish),
      track_alias_(track_alias),
      framer_(framer),
      parameters_(parameters),
      weak_ptr_factory_(this) {
  SessionToPublisherInterface* session_info = visitor_.GetIfAvailable();
  if (session_info == nullptr) {
    return;
  }
  monitoring_interface_ = session_info->ReleaseMonitoringInterface(
      track_publisher_->GetTrackName());
  if (monitoring_interface_ == nullptr && track_publisher_ != nullptr) {
    monitoring_interface_ = track_publisher_->GetMonitoringInterface();
  }
  if (monitoring_interface_ != nullptr) {
    monitoring_interface_->OnObjectAckSupportKnown(parameters.oack_window_size);
  }
  if (is_publish) {
    parameters_.group_order =
        track_publisher_->properties().default_publisher_group_order();
  }
  QUIC_DLOG(INFO) << "Created subscription for "
                  << track_publisher_->GetTrackName();
  // TODO(martinduke): Handle NEW_GROUP_REQUEST
}

LivePublisher::~LivePublisher() {
  if (track_publisher_ != nullptr) {
    track_publisher_->RemoveObjectListener(this);
  }
}

void LivePublisher::Update(const MessageParameters& parameters,
                           bool from_request_ok) {
  absl::Status pub_status =
      track_publisher_->UpdateObjectListener(this, parameters);
  if (!pub_status.ok()) {
    if (!from_request_ok) {
      PublishIsDone(PublishDoneCode::kUpdateFailed, "Update failed");
      bidi_stream_->CheckStatus(
          bidi_stream_->SendRequestError(StatusToMoqtRequestError(pub_status)));
    } else {
      bidi_stream_->Reset(StatusToMoqtStreamError(pub_status));
    }
    return;
  }
  MessageParameters response;
  // TODO(martinduke): If the subscribe window has shrunk, close any streams
  // that are now outside the window. Also send PUBLISH_DONE if now done.
  MoqtPriority old_priority =
      parameters_.subscriber_priority.value_or(kDefaultSubscriberPriority);
  if (!from_request_ok) {
    response.largest_object = track_publisher_->largest_location();
    parameters_.largest_object = response.largest_object;
  }
  parameters_.Update(parameters);
  if (parameters.subscriber_priority.has_value()) {  // priority changed.
    MoqtPriority new_priority = *parameters.subscriber_priority;
    // Reprioritize all active streams.
    for (const webtransport::StreamId stream_id : stream_map_.GetAllStreams()) {
      webtransport::Stream* stream = GetStreamById(stream_id);
      if (stream == nullptr) {
        continue;
      }
      OutgoingSubgroupStream* outgoing_stream =
          absl::down_cast<OutgoingSubgroupStream*>(stream->visitor());
      outgoing_stream->UpdatePriority(new_priority);
    }
    if (!pending_streams_.empty() && visitor() != nullptr) {
      // Tell the session that pending stream priority has changed.
      MoqtPriority publisher_priority =
          pending_streams_.rbegin()->second.publisher_priority.value_or(
              track_publisher_->properties().default_publisher_priority());
      MoqtTrackPriority old_track_priority = {old_priority, publisher_priority};
      visitor()->UpdateTrackPriority(
          track_publisher_->GetTrackName(), old_track_priority,
          MoqtTrackPriority{new_priority, publisher_priority});
    }
  }
  if (!from_request_ok) {
    bidi_stream_->CheckStatus(
        bidi_stream_->SendRequestOk(response, MoqtMessageType::kRequestUpdate));
  }
}

bool LivePublisher::PublishUpdate(const MessageParameters& parameters,
                                  MoqtResponseCallback response_callback) {
  SessionToPublisherInterface* session_info = visitor();
  if (session_info == nullptr) {
    return false;
  }
  bool send_request_update =
      !parameters.authorization_tokens.empty() && is_publish_;
  bool send_notify =
      parameters.location_filter.has_value() || parameters.forward_has_value();
  if (!send_request_update && !send_notify) {
    return false;
  }
  if (send_notify) {
    if (!largest_object_known_) {
      QUICHE_BUG(moqt_publish_notify_before_largest_object_known)
          << "PublishUpdate called before largest_object_known_ is set";
      return false;
    }
    MoqtPublishStateNotify notify;
    notify.parameters.location_filter = parameters.location_filter;
    if (parameters.forward_has_value()) {
      notify.parameters.set_forward(parameters.forward());
    }
    if (track_publisher_ != nullptr) {
      notify.parameters.largest_object = track_publisher_->largest_location();
    }
    // Because notify.parameters is custom-built, calling SanitizeParameters()
    // is a no-op. Don't bother to call it.
    parameters_.Update(notify.parameters);
    if (!bidi_stream_
             ->SendOrBufferMessage(framer_.SerializePublishStateNotify(notify))
             .ok()) {
      return false;
    }
  }
  if (!send_request_update) {
    std::move(response_callback)(MessageParameters());
    return true;
  }
  return bidi_stream_
      ->SendRequestUpdate(session_info->NextRequestId(), parameters,
                          std::move(response_callback),
                          MoqtMessageType::kPublish)
      .ok();
}

void LivePublisher::ResetAllStreams() {
  if (ignore_reset_all_streams_) {
    return;
  }
  for (const webtransport::StreamId stream_id : stream_map_.GetAllStreams()) {
    webtransport::Stream* stream = GetStreamById(stream_id);
    if (stream != nullptr) {
      stream->ResetWithUserCode(kResetCodeCancelled);
    }
  }
}

void LivePublisher::OnSubscribeAccepted() {
  publisher_subgroup_delivery_timeout_ =
      track_publisher_->properties().subgroup_delivery_timeout();
  publisher_object_delivery_timeout_ =
      track_publisher_->properties().object_delivery_timeout();
  default_publisher_priority_ =
      track_publisher_->properties().default_publisher_priority();
  if (is_publish_) {
    return;
  }
  if (largest_object_known_) {
    QUICHE_BUG(moqt_subscribe_accepted_called_twice)
        << "OnSubscribeAccepted called more than once";
    return;
  }
  largest_object_known_ = true;
  parameters_.largest_object = track_publisher_->largest_location();
  if (parameters_.location_filter.has_value() &&
      !parameters_.location_filter->WindowKnown()) {
    parameters_.location_filter->ConvertRelativeToAbsolute(
        parameters_.largest_object);
  }
  MoqtSubscribeOk subscribe_ok;
  subscribe_ok.track_alias = track_alias_;
  subscribe_ok.parameters.expires = track_publisher_->expiration();
  subscribe_ok.parameters.largest_object = parameters_.largest_object;
  subscribe_ok.properties = track_publisher_->properties();
  if (!parameters_.group_order.has_value()) {
    parameters_.group_order =
        subscribe_ok.properties.default_publisher_group_order();
  }
  bidi_stream_->SendOrBufferMessageOrFatal(
      framer_.SerializeSubscribeOk(subscribe_ok));
  // It is now safe to process buffered REQUEST_UPDATEs.
  std::optional<MessageParameters> next_update =
      bidi_stream_->NextIncomingUpdate();
  // Get a weak_pointer to LivePublisher in case it is destroyed by Update().
  quiche::QuicheWeakPtr<LivePublisherInterface> self =
      weak_ptr_factory_.Create();
  while (self.IsValid() && next_update.has_value()) {
    Update(*next_update, /*from_request_ok=*/false);
    if (!self.IsValid()) {
      return;
    }
    next_update = bidi_stream_->NextIncomingUpdate();
  }
  // TODO(martinduke): If we buffer objects that arrived previously, the arrival
  // of the track alias disambiguates what subscription they belong to. Send
  // them.
}

void LivePublisher::OnSubscribeRejected(MoqtRequestErrorInfo info) {
  bidi_stream_->CheckStatus(bidi_stream_->SendRequestError(info));
  // Sending FIN will delete the class.
}

void LivePublisher::OnNewObjectAvailable(Location location,
                                         std::optional<uint64_t> subgroup,
                                         MoqtPriority publisher_priority) {
  if (!InWindow(location)) {
    return;
  }

  if (monitoring_interface_ != nullptr) {
    // Notify the monitoring interface about all newly published normal objects.
    // Objects with other statuses are not guaranteed to be acknowledged, thus
    // passing them into the monitoring interface can lead to confusion.
    std::optional<PublishedObject> object = track_publisher_->GetCachedObject(
        location.group, subgroup, location.object);
    QUICHE_DCHECK(object.has_value())
        << "Object " << absl::StrCat(location) << " on track "
        << track_publisher_->GetTrackName().ToString()
        << " does not exist, despite OnNewObjectAvailable being called";
    if (object.has_value() && object->metadata.location == location &&
        object->metadata.status == MoqtObjectStatus::kNormal) {
      monitoring_interface_->OnNewObjectEnqueued(location);
    }
  }
  // TODO(vasilvv): This currently sends UINT64_MAX for datagram subgroups.
  // Maybe do something more satisfactory?
  SessionToPublisherInterface* session_info = visitor();
  if (session_info == nullptr || session_info->session() == nullptr) {
    return;  // Session is gone.
  }
  session_info->trace_recorder().RecordNewObjectAvaliable(
      track_alias_, *track_publisher_, location, subgroup.value_or(UINT64_MAX),
      publisher_priority);

  std::optional<webtransport::StreamId> stream_id;
  if (subgroup.has_value()) {
    DataStreamIndex index(location.group, *subgroup);
    if (reset_subgroups_.contains(index)) {
      // This subgroup has already been reset, ignore.
      return;
    }
    stream_id = stream_map_.GetStreamFor(index);
  }
  QUICHE_DCHECK_GE(location.group, first_active_group_);
  if (!subgroup.has_value()) {
    SendDatagram(location);
    return;
  }

  webtransport::Stream* raw_stream = nullptr;
  if (stream_id.has_value()) {
    raw_stream = GetStreamById(*stream_id);
    if (raw_stream != nullptr) {
      raw_stream->visitor()->OnCanWrite();
    }
    return;
  }
  NewDataStreamParameters parameters(
      location.group, *subgroup, location.object,
      publisher_priority == default_publisher_priority_
          ? std::nullopt
          : std::make_optional(publisher_priority));
  raw_stream = OpenDataStream(parameters);
  if (raw_stream == nullptr) {
    StreamRank rank = StreamRankFor(parameters);
    if (pending_streams_.empty() || rank > pending_streams_.rbegin()->first) {
      session_info->UpdateTrackPriority(
          track_publisher_->GetTrackName(),
          /*old_priority=*/pending_streams_.empty()
              ? std::optional<MoqtTrackPriority>()
              : std::make_optional(
                    MoqtTrackPriority{subscriber_priority(),
                                      pending_streams_.rbegin()
                                          ->second.publisher_priority.value_or(
                                              default_publisher_priority())}),
          MoqtTrackPriority{subscriber_priority(), publisher_priority});
    }
    // TODO(martinduke): We are not properly enforcing SUBGROUP_DELIVERY_TIMEOUT
    // for pending streams. When LivePublisher receives OnNewFinAvailable(), it
    // should check pending streams (which ought to be accessible by index, not
    // just rank) and start a timer on them.
    pending_streams_.emplace(rank, parameters);
  }
}

void LivePublisher::OnTrackPublisherGone() {
  PublishIsDone(PublishDoneCode::kGoingAway, "Publisher is gone");
}

// TODO(martinduke): Revise to check if the last object has been delivered.
void LivePublisher::OnNewFinAvailable(Location location, uint64_t subgroup) {
  if (!InWindow(location.group)) {
    return;
  }
  DataStreamIndex index(location.group, subgroup);
  std::optional<webtransport::StreamId> stream_id =
      stream_map_.GetStreamFor(index);
  if (!stream_id.has_value()) {
    return;
  }
  webtransport::Stream* raw_stream = GetStreamById(*stream_id);
  if (raw_stream == nullptr) {
    return;
  }
  OutgoingSubgroupStream* stream =
      absl::down_cast<OutgoingSubgroupStream*>(raw_stream->visitor());
  stream->Fin(location);
  // Sending FIN will delete the class.
}

void LivePublisher::OnSubgroupAbandoned(
    uint64_t group, uint64_t subgroup,
    webtransport::StreamErrorCode error_code) {
  if (!InWindow(group) || group < first_active_group_) {
    return;
  }
  DataStreamIndex index(group, subgroup);
  if (reset_subgroups_.contains(index)) {
    // This subgroup has already been reset, ignore.
    return;
  }
  reset_subgroups_.insert(index);
  std::optional<webtransport::StreamId> stream_id =
      stream_map_.GetStreamFor(index);
  if (!stream_id.has_value()) {
    return;
  }
  webtransport::Stream* raw_stream = GetStreamById(*stream_id);
  if (raw_stream == nullptr) {
    return;
  }
  raw_stream->ResetWithUserCode(error_code);
}

void LivePublisher::OnGroupAbandoned(uint64_t group_id) {
  if (!InWindow(group_id)) {
    // The group is not in the window, ignore.
    return;
  }
  std::vector<webtransport::StreamId> streams =
      stream_map_.GetStreamsForGroup(group_id);
  if (subgroup_delivery_timeout().IsInfinite() &&
      object_delivery_timeout().IsInfinite() && largest_sent_.has_value() &&
      largest_sent_->group <= group_id) {
    PublishIsDone(PublishDoneCode::kTooFarBehind, "");
    // No class access below this line!
    return;
  }
  for (webtransport::StreamId stream_id : streams) {
    webtransport::Stream* raw_stream = GetStreamById(stream_id);
    if (raw_stream == nullptr) {
      continue;
    }
    raw_stream->ResetWithUserCode(kResetCodeDeliveryTimeout);
    // Sending the Reset will call the destructor for OutgoingSubgroupStream,
    // which will erase it from the SendStreamMap.
  }
  first_active_group_ = std::max(first_active_group_, group_id + 1);
  absl::erase_if(reset_subgroups_, [&](const DataStreamIndex& index) {
    return index.group < first_active_group_;
  });
}

void LivePublisher::SendDatagram(Location sequence) {
  std::optional<PublishedObject> object = track_publisher_->GetCachedObject(
      sequence.group, std::nullopt, sequence.object);
  if (!object.has_value()) {
    QUICHE_BUG(PublishedSubscription_SendDatagram_object_not_in_cache)
        << "Got notification about an object that is not in the cache";
    return;
  }
  MoqtObject header;
  header.track_alias = track_alias_;
  header.group_id = object->metadata.location.group;
  header.object_id = object->metadata.location.object;
  header.publisher_priority = object->metadata.publisher_priority;
  header.properties = object->metadata.properties;
  header.object_status = object->metadata.status;
  header.subgroup_id = std::nullopt;
  header.payload_length = object->metadata.payload_length;
  QUICHE_BUG_IF(LivePublisher_SendDatagram_partial_payload,
                object->payload.size() > 1)
      << "Datagram is split into multiple slices";
  quiche::QuicheBuffer datagram;
  if (object->payload.empty()) {
    datagram = framer_.SerializeObjectDatagram(
        header, "",
        default_publisher_priority_.value_or(kDefaultPublisherPriority));
  } else {
    datagram = framer_.SerializeObjectDatagram(
        header, object->payload[0].AsStringView(),
        default_publisher_priority_.value_or(kDefaultPublisherPriority));
  }
  if (visitor() == nullptr) {
    return;
  }
  visitor()->session()->SendOrQueueDatagram(datagram.AsStringView());
  OnObjectSent(object->metadata.location);
}

void LivePublisher::ProcessObjectAck(const MoqtObjectAck& message) {
  SessionToPublisherInterface* session_info = visitor();
  if (session_info == nullptr) {
    return;
  }
  session_info->trace_recorder().RecordObjectAck(
      track_alias_, Location(message.group_id, message.object_id),
      message.delta_from_deadline);
  if (monitoring_interface_ != nullptr) {
    monitoring_interface_->OnObjectAckReceived(
        Location(message.group_id, message.object_id),
        message.delta_from_deadline);
  }
}

webtransport::Stream* absl_nullable LivePublisher::OpenDataStream(
    const NewDataStreamParameters& parameters) {
  SessionToPublisherInterface* session_info = visitor();
  if (session_info == nullptr) {
    return nullptr;
  }
  if (!session_info->session()->CanOpenNextOutgoingUnidirectionalStream()) {
    return nullptr;
  }
  webtransport::Stream* new_stream =
      session_info->session()->OpenOutgoingUnidirectionalStream();
  if (new_stream == nullptr) {
    return nullptr;
  }
  stream_map_.AddStream(parameters.index, new_stream->GetStreamId());
  new_stream->SetVisitor(std::make_unique<OutgoingSubgroupStream>(
      framer_, new_stream, parameters.index, parameters.first_object,
      weak_ptr_factory_.Create(), track_publisher_,
      StreamPriorityFor(parameters), track_alias_,
      &session_info->trace_recorder()));
  ++streams_opened_;
  new_stream->visitor()->OnCanWrite();
  return new_stream;
}

void LivePublisher::PublishIsDone(PublishDoneCode code,
                                  absl::string_view error_reason) {
  MoqtPublishDone publish_done;
  publish_done.status_code = code;
  publish_done.stream_count = streams_opened_;
  publish_done.error_reason = error_reason;
  // TODO(martinduke): It is technically correct, but not good, to reset all the
  // streams in order to send PUBLISH_DONE. It's better to wait until streams
  // FIN naturally, where possible.
  QUICHE_DLOG(INFO) << "Sending PUBLISH_DONE message for "
                    << track_publisher_->GetTrackName();
  bidi_stream_->SendOrBufferMessageOrFatal(
      framer_.SerializePublishDone(publish_done),
      // Don't send FIN if the UPDATE failed; REQUEST_ERROR will follow.
      /*fin=*/code != PublishDoneCode::kUpdateFailed);
  // sending FIN will delete the class.
}

void LivePublisher::OnDataStreamDestroyed(DataStreamIndex end_sequence) {
  stream_map_.RemoveStream(end_sequence);
}

void LivePublisher::OnCanCreateNewUniStream() {
  SessionToPublisherInterface* session_info = visitor();
  if (session_info == nullptr) {
    return;
  }
  while (visitor_.IsValid() &&
         session_info->session()->CanOpenNextOutgoingUnidirectionalStream()) {
    auto it = pending_streams_.rbegin();
    while (it != pending_streams_.rend() &&
           (it->second.index.group < first_active_group_ ||
            reset_subgroups_.contains(it->second.index))) {
      pending_streams_.erase(--(it.base()));
      it = pending_streams_.rbegin();
    }
    if (it == pending_streams_.rend()) {
      return;
    }
    if (OpenDataStream(it->second) == nullptr) {
      return;
    }
    pending_streams_.erase(--(it.base()));
    if (!pending_streams_.empty()) {
      session_info->UpdateTrackPriority(
          track_publisher_->GetTrackName(), std::nullopt,
          MoqtTrackPriority{
              subscriber_priority(),
              pending_streams_.rbegin()->second.publisher_priority.value_or(
                  default_publisher_priority())});
    }
  }
}

void LivePublisher::OnObjectSent(Location sequence) {
  if (largest_sent_.has_value()) {
    largest_sent_ = std::max(*largest_sent_, sequence);
  } else {
    largest_sent_ = sequence;
  }
  // TODO: send PUBLISH_DONE if the subscription is done.
}

}  // namespace moqt
