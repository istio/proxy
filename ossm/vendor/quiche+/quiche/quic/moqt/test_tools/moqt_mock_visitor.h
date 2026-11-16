// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MOQT_TOOLS_MOQT_MOCK_VISITOR_H_
#define QUICHE_QUIC_MOQT_TOOLS_MOQT_MOCK_VISITOR_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/moqt_uni_stream.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

struct MockSessionCallbacks {
  testing::MockFunction<void()> session_established_callback;
  testing::MockFunction<void(absl::string_view, quic::QuicTimeDelta)>
      goaway_received_callback;
  testing::MockFunction<void(absl::string_view)> session_terminated_callback;
  testing::MockFunction<void()> session_deleted_callback;
  testing::MockFunction<void(const TrackNamespace&,
                             const MessageParameters* absl_nullable,
                             MoqtResponseCallback)>
      incoming_publish_namespace_callback;
  testing::MockFunction<std::unique_ptr<MoqtNamespaceTask>(
      const TrackNamespace&, const MessageParameters&, MoqtResponseCallback)>
      incoming_subscribe_namespace_callback;
  testing::MockFunction<bool(const TrackNamespace&, const MessageParameters&,
                             MoqtResponseCallback)>
      incoming_subscribe_tracks_callback;

  MockSessionCallbacks() {
    ON_CALL(incoming_publish_namespace_callback, Call)
        .WillByDefault(DefaultIncomingPublishNamespaceCallback);
    ON_CALL(incoming_subscribe_namespace_callback, Call)
        .WillByDefault(DefaultIncomingSubscribeNamespaceCallback);
  }

  MoqtSessionCallbacks AsSessionCallbacks() {
    return MoqtSessionCallbacks{
        session_established_callback.AsStdFunction(),
        goaway_received_callback.AsStdFunction(),
        session_terminated_callback.AsStdFunction(),
        session_deleted_callback.AsStdFunction(),
        incoming_publish_namespace_callback.AsStdFunction(),
        incoming_subscribe_namespace_callback.AsStdFunction()};
  }
};

class MockTrackPublisher : public MoqtTrackPublisher {
 public:
  explicit MockTrackPublisher(FullTrackName name)
      : track_name_(std::move(name)) {
    ON_CALL(*this, properties()).WillByDefault(testing::ReturnRef(properties_));
  }
  const FullTrackName& GetTrackName() const override { return track_name_; }

  MOCK_METHOD(std::optional<PublishedObject>, GetCachedObject,
              (uint64_t, std::optional<uint64_t>, uint64_t, uint64_t),
              (const, override));
  MOCK_METHOD(void, AddObjectListener,
              (MoqtObjectListener * listener,
               const MessageParameters& parameters),
              (override));
  MOCK_METHOD(void, RemoveObjectListener, (MoqtObjectListener * listener),
              (override));
  MOCK_METHOD(absl::Status, UpdateObjectListener,
              (MoqtObjectListener * listener,
               const MessageParameters& parameters),
              (override));
  MOCK_METHOD(std::optional<Location>, largest_location, (), (const, override));
  MOCK_METHOD(const TrackProperties&, properties, (), (const, override));
  MOCK_METHOD(std::optional<quic::QuicTimeDelta>, expiration, (),
              (const, override));
  MOCK_METHOD(std::unique_ptr<MoqtFetchTask>, StandaloneFetch,
              (Location, Location, MoqtGroupOrder, FetchResponseCallback),
              (override));
  MOCK_METHOD(std::unique_ptr<MoqtFetchTask>, RelativeFetch,
              (uint64_t, MoqtGroupOrder, FetchResponseCallback), (override));
  MOCK_METHOD(std::unique_ptr<MoqtFetchTask>, AbsoluteFetch,
              (uint64_t, MoqtGroupOrder, FetchResponseCallback), (override));

 private:
  FullTrackName track_name_;
  const TrackProperties properties_;
};

// A very simple MoqtTrackPublisher that allows tests to add arbitrary objects.
class TestTrackPublisher : public MoqtTrackPublisher {
 public:
  explicit TestTrackPublisher(FullTrackName name)
      : track_name_(std::move(name)) {}
  const FullTrackName& GetTrackName() const override { return track_name_; }
  std::optional<PublishedObject> GetCachedObject(
      uint64_t group, std::optional<uint64_t> subgroup, uint64_t object,
      uint64_t /*offset*/) const override {
    Location location(group, object);
    auto it = objects_.find(location);
    if (it == objects_.end()) {
      return std::nullopt;
    }
    return it->second.ToPublishedObject();
  }
  void AddObjectListener(MoqtObjectListener* listener,
                         const MessageParameters&) override {
    listeners_.insert(listener);
    listener->OnSubscribeAccepted();
  }
  void RemoveObjectListener(MoqtObjectListener* listener) override {
    listeners_.erase(listener);
  }
  absl::Status UpdateObjectListener(MoqtObjectListener*,
                                    const MessageParameters&) override {
    return absl::OkStatus();
  }
  std::optional<Location> largest_location() const override {
    return largest_location_;
  }
  const TrackProperties& properties() const override { return properties_; }
  std::optional<quic::QuicTimeDelta> expiration() const override {
    return quic::QuicTimeDelta::Infinite();
  }
  // TODO(martinduke): Support Fetch
  std::unique_ptr<MoqtFetchTask> StandaloneFetch(
      Location start, Location end, MoqtGroupOrder group_order,
      FetchResponseCallback callback) override {
    std::move(callback)(MoqtRequestErrorInfo{
        .error_code = RequestErrorCode::kDoesNotExist,
        .reason_phrase = "Fetch not implemented",
    });
    return nullptr;
  }
  std::unique_ptr<MoqtFetchTask> RelativeFetch(
      uint64_t offset, MoqtGroupOrder group_order,
      FetchResponseCallback callback) override {
    std::move(callback)(MoqtRequestErrorInfo{
        .error_code = RequestErrorCode::kDoesNotExist,
        .reason_phrase = "Fetch not implemented",
    });
    return nullptr;
  }
  std::unique_ptr<MoqtFetchTask> AbsoluteFetch(
      uint64_t offset, MoqtGroupOrder group_order,
      FetchResponseCallback callback) override {
    std::move(callback)(MoqtRequestErrorInfo{
        .error_code = RequestErrorCode::kDoesNotExist,
        .reason_phrase = "Fetch not implemented",
    });
    return nullptr;
  }
  void AddObject(Location location, uint64_t subgroup,
                 absl::string_view payload, bool fin,
                 quic::QuicTime arrival_time = quic::QuicTime::Zero()) {
    PublishedObjectMetadata metadata;
    metadata.location = location;
    metadata.subgroup = subgroup;
    metadata.properties = "";
    metadata.status = MoqtObjectStatus::kNormal;
    metadata.publisher_priority = 128;
    metadata.first_object_in_subgroup = location.object == 0;
    metadata.payload_length = payload.length();
    metadata.arrival_time = arrival_time;
    auto it = objects_.find(location);
    if (it != objects_.end()) {
      it->second.Append(it->second.payload_received(), payload);
    } else {
      objects_.try_emplace(location, metadata,
                           quiche::QuicheMemSlice::Copy(payload), fin);
    }
    if (!largest_location_.has_value() || *largest_location_ < location) {
      largest_location_ = location;
    }
    for (MoqtObjectListener* listener : listeners_) {
      if (fin) {
        listener->OnNewFinAvailable(location, subgroup);
      }
      listener->OnNewObjectAvailable(location, subgroup, 128);
    }
  }
  void RemoveAllSubscriptions() {
    while (!listeners_.empty()) {
      (*listeners_.begin())->OnTrackPublisherGone();
    }
  }

 private:
  FullTrackName track_name_;
  absl::flat_hash_set<MoqtObjectListener*> listeners_;
  std::map<Location, CachedObject> objects_;
  std::optional<Location> largest_location_;
  TrackProperties properties_;
};

// TODO(martinduke): Rename to MockSubscribeVisitor.
class MockLiveSubscriberVisitor : public SubscribeVisitor {
 public:
  MOCK_METHOD(void, OnReply,
              (const FullTrackName& full_track_name,
               (std::variant<SubscribeOkData, MoqtRequestErrorInfo> response)),
              (override));
  MOCK_METHOD(void, OnCanAckObjects, (MoqtObjectAckFunction ack_function),
              (override));
  MOCK_METHOD(void, OnObjectFragment,
              (const FullTrackName& full_track_name,
               const PublishedObjectMetadata& metadata,
               absl::string_view object, uint64_t offset),
              (override));
  MOCK_METHOD(void, OnPublishDone, (FullTrackName full_track_name), (override));
  MOCK_METHOD(void, OnMalformedTrack, (const FullTrackName& full_track_name),
              (override));
  MOCK_METHOD(void, OnStreamFin,
              (const FullTrackName& full_track_name, DataStreamIndex stream),
              (override));
  MOCK_METHOD(void, OnStreamReset,
              (const FullTrackName& full_track_name, DataStreamIndex stream),
              (override));
};

class MockPublishingMonitorInterface : public MoqtPublishingMonitorInterface {
 public:
  MOCK_METHOD(void, OnObjectAckSupportKnown,
              (std::optional<quic::QuicTimeDelta> time_window), (override));
  MOCK_METHOD(void, OnNewObjectEnqueued, (Location location), (override));
  MOCK_METHOD(void, OnObjectAckReceived,
              (Location location, quic::QuicTimeDelta delta_from_deadline),
              (override));
};

class MockFetchTask : public MoqtFetchTask {
 public:
  MockFetchTask() {};  // No synchronous callbacks.
  explicit MockFetchTask(bool synchronous_object_available)
      : synchronous_object_available_(synchronous_object_available) {}

  MOCK_METHOD(MoqtFetchTask::GetNextObjectResult, GetNextObject,
              (PublishedObject & output), (override));
  MOCK_METHOD(absl::Status, GetStatus, (), (override));

  void SetObjectAvailableCallback(ObjectsAvailableCallback callback) override {
    objects_available_callback_ = std::move(callback);
    if (synchronous_object_available_) {
      // The first call is installed by the session to trigger stream creation.
      // An object might not exist yet.
      objects_available_callback_();
      // This class could be destroyed by the line above.
      return;
    }
    // The second call is a result of the stream replacing the callback, which
    // means there is an object available.
    synchronous_object_available_ = true;
  }

  void CallObjectsAvailableCallback() { objects_available_callback_(); };

 private:
  ObjectsAvailableCallback objects_available_callback_;
  bool synchronous_object_available_ = false;
};

class MockUpstreamFetchTask : public UpstreamFetchTask {
 public:
  MockUpstreamFetchTask() {
    ON_CALL(*this, HasObject).WillByDefault([this]() {
      return UpstreamFetchTask::HasObject();
    });
    ON_CALL(*this, NeedsMorePayload).WillByDefault([this]() {
      return UpstreamFetchTask::NeedsMorePayload();
    });
  }
  ~MockUpstreamFetchTask() override = default;

  MOCK_METHOD(void, set_can_read_callback, (CanReadCallback callback),
              (override));
  MOCK_METHOD(void, set_task_destroyed_callback,
              (TaskDestroyedCallback callback), (override));
  MOCK_METHOD(void, NewObject, (const MoqtObject& message), (override));
  MOCK_METHOD(void, AppendPayloadToObject, (absl::string_view payload),
              (override));
  MOCK_METHOD(bool, HasObject, (), (const, override));
  MOCK_METHOD(bool, NeedsMorePayload, (), (const, override));
  MOCK_METHOD(void, NotifyNewObject, (), (override));
  MOCK_METHOD(void, OnStreamAndFetchClosed, (absl::Status status), (override));
};

class MockSessionToUniStreamInterface : public SessionToUniStreamInterface {
 public:
  MockSessionToUniStreamInterface() = default;
  ~MockSessionToUniStreamInterface() override = default;

  MOCK_METHOD(size_t, max_undelivered_object, (), (const, override));
  MOCK_METHOD(void, OnMalformedTrack, (ObjectSubscriber*), (override));
  MOCK_METHOD(quiche::QuicheWeakPtr<ObjectSubscriber>, GetSubscribe, (uint64_t),
              (override));
  MOCK_METHOD(quiche::QuicheWeakPtr<ObjectSubscriber>, GetFetch, (uint64_t),
              (override));
  MOCK_METHOD(void, Error, (MoqtError, absl::string_view), (override));
};

class MockNamespaceTask : public MoqtNamespaceTask {
 public:
  explicit MockNamespaceTask(const TrackNamespace& prefix)
      : prefix_(prefix), weak_ptr_factory_(this) {}
  void SetObjectsAvailableCallback(ObjectsAvailableCallback
                                   absl_nullable callback) override {
    callback_ = std::move(callback);
  }
  MOCK_METHOD(GetNextResult, GetNextSuffix,
              (TrackNamespace & whole_namespace, TransactionType& type),
              (override));
  MOCK_METHOD(std::optional<webtransport::StreamErrorCode>, GetStatus, (),
              (override));
  const TrackNamespace& prefix() override { return prefix_; }
  MOCK_METHOD(void, Update,
              (const MessageParameters& parameters,
               MoqtResponseCallback response_callback),
              (override));

  void InvokeCallback() {
    if (callback_ != nullptr) {
      callback_();
    }
  }
  quiche::QuicheWeakPtr<MockNamespaceTask> GetWeakPtr() {
    return weak_ptr_factory_.Create();
  }

 private:
  ObjectsAvailableCallback callback_;
  TrackNamespace prefix_;
  quiche::QuicheWeakPtrFactory<MockNamespaceTask> weak_ptr_factory_;
};

class MockMoqtObjectListener : public MoqtObjectListener {
 public:
  MOCK_METHOD(void, OnSubscribeAccepted, (), (override));
  MOCK_METHOD(void, OnSubscribeRejected, (MoqtRequestErrorInfo), (override));
  MOCK_METHOD(void, OnNewObjectAvailable,
              (Location, std::optional<uint64_t>, MoqtPriority), (override));
  MOCK_METHOD(void, OnNewFinAvailable, (Location, uint64_t), (override));
  MOCK_METHOD(void, OnSubgroupAbandoned,
              (uint64_t, uint64_t, webtransport::StreamErrorCode), (override));
  MOCK_METHOD(void, OnGroupAbandoned, (uint64_t), (override));
  MOCK_METHOD(void, OnTrackPublisherGone, (), (override));
};

}  // namespace moqt::test

#endif  // QUICHE_QUIC_MOQT_TOOLS_MOQT_MOCK_VISITOR_H_
