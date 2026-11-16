// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MOQT_MOQT_SUBSCRIPTION_H_
#define QUICHE_QUIC_MOQT_MOQT_SUBSCRIPTION_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_stream_map.h"
#include "quiche/quic/moqt/moqt_trace_recorder.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/common/platform/api/quiche_export.h"
#include "quiche/common/quiche_callbacks.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

namespace test {
class LivePublisherPeer;
}  // namespace test

// This is the part of the send order useful for ranking streams within the
// subscription. It sets the subscriber_priority to kDefaultSubscriberPriority
// to avoid constantly updating all pending streams.
using StreamRank = webtransport::SendOrder;

struct NewDataStreamParameters {
  DataStreamIndex index;
  uint64_t first_object;
  // nullopt if the default priority is used.
  std::optional<MoqtPriority> publisher_priority;

  NewDataStreamParameters(uint64_t group, uint64_t subgroup,
                          uint64_t first_object,
                          std::optional<MoqtPriority> publisher_priority)
      : index(group, subgroup),
        first_object(first_object),
        publisher_priority(publisher_priority) {}
};

// MoqtPublishingMonitorInterface allows a publisher monitor the delivery
// progress for a single individual subscriber.
class MoqtPublishingMonitorInterface {
 public:
  virtual ~MoqtPublishingMonitorInterface() = default;

  virtual void OnObjectAckSupportKnown(
      std::optional<quic::QuicTimeDelta> time_window) = 0;
  virtual void OnNewObjectEnqueued(Location location) = 0;
  virtual void OnObjectAckReceived(Location location,
                                   quic::QuicTimeDelta delta_from_deadline) = 0;
};

// Allows LivePublisher to get data from the session.
class QUICHE_EXPORT SessionToPublisherInterface {
 public:
  virtual ~SessionToPublisherInterface() = default;
  // If |old_priority| is nullopt, the subscription does not have any pending
  // streams. If it has a value, |old_priority| is the old value to be
  // replaced by |new_priority|.
  virtual void UpdateTrackPriority(
      const FullTrackName& name, std::optional<MoqtTrackPriority> old_priority,
      MoqtTrackPriority new_priority) = 0;
  virtual quic::QuicAlarmFactory* alarm_factory() = 0;
  virtual std::shared_ptr<MoqtTrackPublisher> GetTrackPublisher(
      const FullTrackName& name) = 0;
  virtual MoqtPublishingMonitorInterface* absl_nullable
  ReleaseMonitoringInterface(const FullTrackName& name) = 0;
  virtual const quic::QuicClock* clock() = 0;
  virtual MoqtTraceRecorder& trace_recorder() = 0;
  virtual webtransport::Session* session() = 0;
  virtual uint64_t NextRequestId() = 0;
};

// State for delivery of objects via a subscription, whether initiated by a
// SUBSCRIBE or PUBLISH.
class LivePublisher : public MoqtObjectListener, public LivePublisherInterface {
 public:
  // The provider of this callback will add/delete whatever state it is tracking
  // for the subscription. This will be used by both PUBLISH and SUBSCRIBE
  // streams. AddCallback returns |false| if the add fails because the key
  // already exists.
  using AddCallback = quiche::SingleUseCallback<bool(LivePublisher*)>;
  using RemoveCallback = quiche::SingleUseCallback<void(LivePublisher*)>;
  LivePublisher(MoqtFramer framer,
                std::shared_ptr<MoqtTrackPublisher> track_publisher,
                MoqtBidiStreamBase* absl_nonnull bidi_stream,
                uint64_t request_id, uint64_t track_alias,
                const MessageParameters& parameters,
                quiche::QuicheWeakPtr<SessionToPublisherInterface> visitor,
                bool is_publish);
  ~LivePublisher();

  LivePublisher(const LivePublisher&) = delete;
  LivePublisher(LivePublisher&&) = delete;
  LivePublisher& operator=(const LivePublisher&) = delete;
  LivePublisher& operator=(LivePublisher&&) = delete;

  uint64_t request_id() const { return request_id_; }
  MoqtTrackPublisher& publisher() { return *track_publisher_; }
  uint64_t track_alias() const { return track_alias_; }
  MessageParameters& parameters() { return parameters_; }

  // MoqtObjectListener implementation.
  void OnSubscribeAccepted() override;
  void OnSubscribeRejected(MoqtRequestErrorInfo info) override;
  // This is only called for objects that have just arrived.
  void OnNewObjectAvailable(Location location, std::optional<uint64_t> subgroup,
                            MoqtPriority publisher_priority) override;
  void OnTrackPublisherGone() override;
  void OnNewFinAvailable(Location location, uint64_t subgroup) override;
  // also a part of LivePublisherInterface.
  void OnSubgroupAbandoned(uint64_t group, uint64_t subgroup,
                           webtransport::StreamErrorCode error_code) override;
  void OnGroupAbandoned(uint64_t group_id) override;
  void ProcessObjectAck(const MoqtObjectAck& message);

  // LivePublisherInterface implementation.
  bool InWindow(Location location) override {
    return parameters_.forward() &&
           (!parameters_.location_filter.has_value() ||
            (parameters_.location_filter->WindowKnown() &&
             parameters_.location_filter->InWindow(location)));
  };
  const quic::QuicClock* clock() override {
    if (visitor() == nullptr) {
      return nullptr;
    }
    return visitor()->clock();
  }
  quic::QuicTimeDelta object_delivery_timeout() override {
    return std::min(
        parameters_.object_delivery_timeout.value_or(kDefaultDeliveryTimeout),
        publisher_object_delivery_timeout_);
  }
  quic::QuicTimeDelta subgroup_delivery_timeout() override {
    return std::min(
        parameters_.subgroup_delivery_timeout.value_or(kDefaultDeliveryTimeout),
        publisher_subgroup_delivery_timeout_);
  }
  quic::QuicAlarmFactory* alarm_factory() override {
    if (visitor() == nullptr) {
      return nullptr;
    }
    return visitor()->alarm_factory();
  }
  void OnObjectSent(Location sequence) override;
  void OnStreamTimeout(DataStreamIndex index) override {
    reset_subgroups_.insert(index);
  }
  // OnSubgroupAbandoned() is declared above with MoqtObjectListener.
  void OnDataStreamDestroyed(DataStreamIndex) override;

  // MoqtObjectPublisher implementation.
  void OnCanCreateNewUniStream();

  // Called when the parameters_ need an update and will generate REQUEST_OK or
  // REQUEST_ERROR, unless from_request_ok is true (which requires no response).
  void Update(const MessageParameters& parameters, bool from_request_ok);
  // Change the parameters for the subscription, whether initiated by PUBLISH or
  // SUBSCRIBE. Sends REQUEST_UPDATE and/or PUBLISH_STATE_NOTIFY depending on
  // which |parameters| are included.
  // Returns true if all required messages were sent, and |response_callback|
  // will be (or has already been) invoked.
  bool PublishUpdate(const MessageParameters& parameters,
                     MoqtResponseCallback response_callback);

  bool can_have_joining_fetch() const { return parameters_.forward(); }

  bool largest_object_known() const { return largest_object_known_; }

  quiche::QuicheWeakPtr<LivePublisherInterface> GetWeakPtr() {
    return weak_ptr_factory_.Create();
  }

  // Resets all unidirectional streams.
  void ResetAllStreams();
  // Called when the bidi stream is being destroyed. If the result of a session
  // teardown, it is not safe to access the streams via GetStreamById, and the
  // uni streams will be destroyed anyway.
  void IgnoreResetAllStreams() { ignore_reset_all_streams_ = true; }

 private:
  friend class test::LivePublisherPeer;

  MoqtPriority default_publisher_priority() const {
    return default_publisher_priority_.value_or(kDefaultPublisherPriority);
  }

  // Checks if a given Location or Group should be forwarded to the
  // subscriber.
  bool InWindow(uint64_t group) {
    return parameters_.forward() &&
           (!parameters_.location_filter.has_value() ||
            (parameters_.location_filter->WindowKnown() &&
             parameters_.location_filter->InWindow(group)));
  }

  void SendDatagram(Location sequence);

  // Returns the rank of the stream with respect to other streams in the
  // subscription. Higher numbers are higher priority.
  StreamRank StreamRankFor(const NewDataStreamParameters& parameters) const {
    return SendOrderForStream(
        kDefaultSubscriberPriority,
        parameters.publisher_priority.value_or(default_publisher_priority()),
        parameters.index.group, parameters.index.subgroup,
        *parameters_.group_order);
  }

  // Returns the stream priority for use at the moment of stream creation.
  webtransport::StreamPriority StreamPriorityFor(
      const NewDataStreamParameters& parameters) const {
    return webtransport::StreamPriority{
        kMoqtSendGroupId,
        SendOrderForStream(subscriber_priority(),
                           parameters.publisher_priority.value_or(
                               default_publisher_priority()),
                           parameters.index.group, parameters.index.subgroup,
                           *parameters_.group_order)};
  }

  webtransport::Stream* absl_nullable OpenDataStream(
      const NewDataStreamParameters& parameters);

  void PublishIsDone(PublishDoneCode code, absl::string_view reason);

  MoqtPriority subscriber_priority() const {
    return parameters_.subscriber_priority.value_or(kDefaultSubscriberPriority);
  }

  webtransport::Stream* GetStreamById(webtransport::StreamId stream_id) {
    if (visitor() != nullptr && visitor()->session() != nullptr) {
      return visitor()->session()->GetStreamById(stream_id);
    }
    return nullptr;
  }

  // If nullptr, MoqtSession is gone.
  SessionToPublisherInterface* absl_nullable visitor() const {
    return visitor_.GetIfAvailable();
  }

  std::shared_ptr<MoqtTrackPublisher> track_publisher_;
  MoqtBidiStreamBase* absl_nonnull bidi_stream_;
  quiche::QuicheWeakPtr<SessionToPublisherInterface> visitor_;
  uint64_t request_id_;
  // True once LARGEST_OBJECT has been determined (always true for PUBLISH, and
  // set on SUBSCRIBE_OK for SUBSCRIBE).
  bool largest_object_known_;
  const bool is_publish_;
  const uint64_t track_alias_;
  MoqtFramer framer_;
  // These are (mostly) the parameters from the SUBSCRIBE message. However,
  // group_order and largest_object may be updated by SUBSCRIBE_OK because
  // have no effect in a future REQUEST_UPDATE message.
  MessageParameters parameters_;
  quic::QuicTimeDelta publisher_object_delivery_timeout_ =
      kDefaultDeliveryTimeout;
  quic::QuicTimeDelta publisher_subgroup_delivery_timeout_ =
      kDefaultDeliveryTimeout;
  std::optional<MoqtPriority> default_publisher_priority_;
  uint64_t streams_opened_ = 0;

  // The subscription will ignore any groups with a lower ID, so it doesn't
  // need to track reset subgroups.
  uint64_t first_active_group_ = 0;
  // If a stream has been reset due to delivery timeout, do not open a new
  // stream if more object arrive for it.
  absl::flat_hash_set<DataStreamIndex> reset_subgroups_;

  // Monitoring interface that notifies the track publisher about received
  // object ACKs; can be provided either through the session or through the
  // track publisher.
  // TODO(vasilvv): clean up this API.
  MoqtPublishingMonitorInterface* absl_nullable monitoring_interface_ = nullptr;
  // Largest sequence number ever sent via this subscription.
  std::optional<Location> largest_sent_;
  SendStreamMap stream_map_;
  bool ignore_reset_all_streams_ = false;
  // Store the StreamRank of queued outgoing data streams. High StreamRank is
  // highest priority, so use rbegin() to get the highest priority pending
  // stream.
  absl::btree_multimap<StreamRank, NewDataStreamParameters> pending_streams_;
  // Must be last.
  quiche::QuicheWeakPtrFactory<LivePublisherInterface> weak_ptr_factory_;
};

}  // namespace moqt

#endif  // QUICHE_QUIC_MOQT_MOQT_SUBSCRIPTION_H_
