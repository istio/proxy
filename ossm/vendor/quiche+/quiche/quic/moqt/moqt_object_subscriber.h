// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(martinduke): Rename this file to moqt_object_subscriber.h

#ifndef QUICHE_QUIC_MOQT_MOQT_TRACK_H_
#define QUICHE_QUIC_MOQT_MOQT_TRACK_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/quiche_callbacks.h"
#include "quiche/common/quiche_circular_deque.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

namespace test {
class MoqtSessionPeer;
class LiveSubscriberPeer;
}  // namespace test

// State common to both SUBSCRIBE and FETCH upstream.
class ObjectSubscriber {
 public:
  ObjectSubscriber(const FullTrackName& full_track_name, uint64_t id,
                   const MessageParameters& parameters,
                   MoqtBidiStreamBase* request_stream)
      : full_track_name_(full_track_name),
        request_id_(id),
        request_stream_(request_stream),
        parameters_(parameters),
        weak_ptr_factory_(this) {}
  virtual ~ObjectSubscriber() {}

  const FullTrackName& full_track_name() const { return full_track_name_; }
  uint64_t request_id() const { return request_id_; }

  virtual void OnStreamOpened(webtransport::StreamVisitor* stream) = 0;
  virtual void OnStreamClosed(absl::Status status,
                              std::optional<DataStreamIndex> index) = 0;

  // Is the object one that was requested?
  virtual bool InWindow(Location sequence) const = 0;

  quiche::QuicheWeakPtr<ObjectSubscriber> weak_ptr() {
    return weak_ptr_factory_.Create();
  }

  virtual bool is_fetch() const = 0;

  // A REQUEST_UPDATE or PUBLISH_STATE_NOTIFY changes any field that is present
  // in |parameters|. If status is not OK, there is a protocol violation.
  absl::Status Update(const MessageParameters& parameters) {
    if (parameters.largest_object.has_value() &&
        parameters_.largest_object > parameters.largest_object) {
      return absl::InvalidArgumentError("LARGEST_OBJECT cannot be decreased");
    }
    parameters_.Update(parameters);
    return absl::OkStatus();
  }

  MoqtBidiStreamBase* request_stream() { return request_stream_; }

  const MessageParameters& const_parameters() const { return parameters_; }

 protected:
  MessageParameters& parameters() { return parameters_; }

 private:
  const FullTrackName full_track_name_;
  const uint64_t request_id_;
  MoqtBidiStreamBase* request_stream_;
  MessageParameters parameters_;

  // Must be last.
  quiche::QuicheWeakPtrFactory<ObjectSubscriber> weak_ptr_factory_;
};

// A track on the peer to which the session has subscribed.
class LiveSubscriber : public ObjectSubscriber {
 public:
  // Returns the existing subscription, if present.
  using AddCallback = quiche::SingleUseCallback<bool(LiveSubscriber*)>;
  using RemoveCallback = quiche::SingleUseCallback<void(LiveSubscriber*)>;
  LiveSubscriber(const MoqtSubscribe& subscribe, SubscribeVisitor* visitor,
                 MoqtBidiStreamBase* request_stream)
      : ObjectSubscriber(subscribe.full_track_name, subscribe.request_id,
                         subscribe.parameters, request_stream),
        visitor_(visitor) {}
  LiveSubscriber(const MoqtPublish& publish, SubscribeVisitor* visitor,
                 MoqtBidiStreamBase* request_stream)
      : ObjectSubscriber(publish.full_track_name, publish.request_id,
                         publish.parameters, request_stream),
        visitor_(visitor) {
    track_alias_.emplace(publish.track_alias);
  }
  ~LiveSubscriber() override;

  void OnObjectOrOk(const SubscribeOkData& data);
  void OnObjectOrOk() { error_is_allowed_ = false; }
  // If REQUEST_ERROR arrives after OK or an object, it is a protocol violation.
  bool ErrorIsAllowed() const { return error_is_allowed_; }
  std::optional<uint64_t> track_alias() const { return track_alias_; }
  // Returns false if the callback returns false, meaning the session has been
  // destroyed.
  void set_track_alias(uint64_t track_alias) {
    track_alias_.emplace(track_alias);
  }
  void OnStreamOpened(webtransport::StreamVisitor* /*unused*/) override;
  // If |status.ok()|, it was a FIN.
  void OnStreamClosed(absl::Status status,
                      std::optional<DataStreamIndex> index) override;
  void OnPublishDone(uint64_t stream_count, const quic::QuicClock* clock,
                     quic::QuicAlarmFactory* alarm_factory);

  // The application can request a Joining FETCH but also for FETCH objects to
  // be delivered via LiveSubscriber::Visitor::OnObjectFragment(). When
  // this occurs, the session passes the FetchTask here to handle incoming
  // FETCH objects to pipe directly into the visitor.
  void OnJoiningFetchReady(std::unique_ptr<MoqtFetchTask> fetch_task);

  bool is_fetch() const override { return false; }

  bool InWindow(Location location) const override {
    return const_parameters().forward() &&
           (!const_parameters().location_filter.has_value() ||
            const_parameters().location_filter->InWindow(location));
  }

  MoqtPriority default_publisher_priority() const {
    return default_publisher_priority_;
  }

  quic::QuicTimeDelta publisher_object_delivery_timeout() const {
    return publisher_object_delivery_timeout_;
  }
  quic::QuicTimeDelta publisher_subgroup_delivery_timeout() const {
    return publisher_subgroup_delivery_timeout_;
  }
  quic::QuicTimeDelta object_delivery_timeout() const {
    return std::min(const_parameters().object_delivery_timeout.value_or(
                        kDefaultDeliveryTimeout),
                    publisher_object_delivery_timeout_);
  }
  quic::QuicTimeDelta subgroup_delivery_timeout() const {
    return std::min(const_parameters().subgroup_delivery_timeout.value_or(
                        kDefaultDeliveryTimeout),
                    publisher_subgroup_delivery_timeout_);
  }

  SubscribeVisitor* visitor() const { return visitor_; }
  SubscribeVisitor* ReleaseVisitor() {
    SubscribeVisitor* temp = visitor_;
    visitor_ = nullptr;
    return temp;
  }
  void set_visitor(SubscribeVisitor* visitor) { visitor_ = visitor; }

  void SendObjectAck(uint64_t group_id, uint64_t object_id,
                     quic::QuicTimeDelta delta_from_deadline);

  bool dynamic_groups() const { return dynamic_groups_; }

 private:
  friend class test::MoqtSessionPeer;
  friend class test::LiveSubscriberPeer;

  class PublishDoneDelegate : public quic::QuicAlarm::DelegateWithoutContext {
   public:
    PublishDoneDelegate(quiche::QuicheWeakPtr<ObjectSubscriber> subscribe)
        : subscribe_(subscribe) {}

    void OnAlarm() override {
      ObjectSubscriber* subscribe = subscribe_.GetIfAvailable();
      if (subscribe == nullptr) {
        return;
      }
      subscribe->request_stream()->Reset(kResetCodeCancelled);
    }

   private:
    quiche::QuicheWeakPtr<ObjectSubscriber> subscribe_;
  };

  void MaybeSetPublishDoneAlarm();
  bool all_streams_closed() const {
    return total_streams_.has_value() && *total_streams_ == streams_closed_;
  }
  // If false, an object or OK message has been received, so any ERROR message
  // is a protocol violation.
  bool error_is_allowed_ = true;

  quic::QuicTimeDelta publisher_object_delivery_timeout_ =
      kDefaultDeliveryTimeout;
  quic::QuicTimeDelta publisher_subgroup_delivery_timeout_ =
      kDefaultDeliveryTimeout;
  MoqtPriority default_publisher_priority_ = kDefaultPublisherPriority;
  bool dynamic_groups_ = kDefaultDynamicGroups;
  void FetchObjects();
  std::unique_ptr<MoqtFetchTask> fetch_task_;
  // If nonzero, fetch_task_ is in mid-object.
  uint64_t fetch_object_offset_ = 0;

  std::optional<const uint64_t> track_alias_;
  SubscribeVisitor* visitor_;
  int currently_open_streams_ = 0;
  // Every stream that has received FIN or RESET_STREAM.
  uint64_t streams_closed_ = 0;
  // Value assigned on PUBLISH_DONE. Can destroy subscription state if
  // streams_closed_ == total_streams_.
  std::optional<uint64_t> total_streams_;
  std::unique_ptr<quic::QuicAlarm> publish_done_alarm_ = nullptr;
  const quic::QuicClock* clock_ = nullptr;
};

// This is a callback to MoqtSession::IncomingDataStream. Called when the
// FetchTask has its object cache empty, on creation, and whenever the
// application reads it.
using CanReadCallback = quiche::MultiUseCallback<void()>;

// If the application destroys the FetchTask, this is a signal to
// the owner to cancel the FETCH and STOP_SENDING the stream.
using TaskDestroyedCallback = quiche::SingleUseCallback<void()>;

// This class is passed to the application, which views it as a MoqtFetchTask.
// A pointer to the child calls is held by the FETCH control at first, then the
// data stream when initiated, to update its state. UpstreamFetchTask is
// responsible for calling task_destroyed_callback_ so that pointers to it are
// cleared, so there is no need for a QuicheWeakPtr.
class QUICHE_EXPORT UpstreamFetchTask : public MoqtFetchTask {
 public:
  // If the FetchRequestStream is destroyed, it will call OnStreamAndFetchClosed
  // which sets the TaskDestroyedCallback to nullptr. Thus, |callback| can
  // assume that FetchRequestStream is valid.
  UpstreamFetchTask() {}
  ~UpstreamFetchTask() override;

  // Implementation of MoqtFetchTask.
  GetNextObjectResult GetNextObject(PublishedObject& output) override;
  void SetObjectAvailableCallback(ObjectsAvailableCallback callback) override {
    object_available_callback_ = std::move(callback);
  };
  absl::Status GetStatus() override { return status_; };

  // Called by incoming data stream.
  virtual void set_can_read_callback(CanReadCallback callback) {
    can_read_callback_ = std::move(callback);
    if (can_read_callback_) {
      can_read_callback_();  // Accept the first object.
    }
  }
  virtual void set_task_destroyed_callback(TaskDestroyedCallback callback) {
    task_destroyed_callback_ = std::move(callback);
  }

  // Called when the data stream receives a new object.
  virtual void NewObject(const MoqtObject& message);
  virtual void AppendPayloadToObject(absl::string_view payload);
  // The data stream calls this for a hint if the object has been read.
  virtual bool HasObject() const { return next_object_.has_value(); }
  virtual bool NeedsMorePayload() const {
    return next_object_.has_value() &&
           payload_length_ < next_object_->payload_length;
  }
  // The data stream calls NotifyNewObject() after NewObject() because it has to
  // exit the parser loop before the callback possibly causes another read.
  // Furthermore, NewObject() may be a partial object, and so
  // NotifyNewObject() is called only when the object is complete.
  virtual void NotifyNewObject();

  // Deletes callbacks to session or stream, updates the status. If |status| is
  // OK, will append an EOF to the object stream.
  virtual void OnStreamAndFetchClosed(absl::Status status);

  virtual uint64_t payload_offset() const { return payload_offset_; }
  virtual uint64_t payload_length() const { return payload_length_; }

 private:
  absl::Status status_ = absl::OkStatus();
  TaskDestroyedCallback task_destroyed_callback_;

  // Object delivery state. The payload_length member is used to track the
  // payload bytes not yet received. The application receives a
  // PublishedObject that is constructed from next_object_ and payload_.
  std::optional<MoqtObject> next_object_;
  quiche::QuicheCircularDeque<quiche::QuicheMemSlice> payload_;
  // The starting point of payload_. Data is deleted as it is delivered.
  uint64_t payload_offset_ = 0;
  // Total data delivered for this object.
  uint64_t payload_length_ = 0;

  // The task should only call object_available_callback_ when the last result
  // was kPending. Otherwise, there can be recursive loops of
  // GetNextObjectResult().
  bool need_object_available_callback_ = true;
  bool eof_ = false;  // The next object is EOF.
  // The Fetch task signals the application when it has new objects.
  ObjectsAvailableCallback object_available_callback_;
  // The Fetch task signals the stream when it has dispensed of an object.
  CanReadCallback can_read_callback_;
};

}  // namespace moqt

#endif  // QUICHE_QUIC_MOQT_MOQT_TRACK_H_
