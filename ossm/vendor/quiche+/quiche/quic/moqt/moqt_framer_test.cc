// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_framer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_test_message.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {

struct MoqtFramerTestParams {
  MoqtFramerTestParams(MoqtMessageType message_type, bool uses_web_transport,
                       quic::Perspective perspective)
      : message_type(message_type),
        uses_web_transport(uses_web_transport),
        perspective(perspective) {}
  MoqtMessageType message_type;
  bool uses_web_transport;
  quic::Perspective perspective;
};

std::vector<MoqtFramerTestParams> GetMoqtFramerTestParams() {
  std::vector<MoqtFramerTestParams> params;
  std::vector<MoqtMessageType> message_types = {
      MoqtMessageType::kRequestOk,
      MoqtMessageType::kRequestError,
      MoqtMessageType::kSubscribe,
      MoqtMessageType::kSubscribeOk,
      MoqtMessageType::kRequestUpdate,
      MoqtMessageType::kPublishDone,
      MoqtMessageType::kPublishNamespace,
      MoqtMessageType::kNamespace,
      MoqtMessageType::kNamespaceDone,
      MoqtMessageType::kPublishSkipped,
      MoqtMessageType::kTrackStatus,
      MoqtMessageType::kGoAway,
      MoqtMessageType::kSubscribeNamespace,
      MoqtMessageType::kSubscribeTracks,
      MoqtMessageType::kFetch,
      MoqtMessageType::kFetchOk,
      MoqtMessageType::kPublish,
      MoqtMessageType::kPublishStateNotify,
      MoqtMessageType::kObjectAck,
      MoqtMessageType::kSetup,
  };
  for (const MoqtMessageType message_type : message_types) {
    if (message_type == MoqtMessageType::kSetup) {
      for (const bool uses_web_transport : {false, true}) {
        for (const quic::Perspective perspective :
             {quic::Perspective::IS_CLIENT, quic::Perspective::IS_SERVER}) {
          params.push_back(MoqtFramerTestParams(
              message_type, uses_web_transport, perspective));
        }
      }
    } else if (message_type == MoqtMessageType::kGoAway) {
      // GoAwayMessage has a non-empty new_session_uri, which is only valid
      // when sent by the server.
      params.push_back(MoqtFramerTestParams(message_type, true,
                                            quic::Perspective::IS_SERVER));
    } else {
      // All other types are processed the same for either perspective or
      // transport.
      params.push_back(MoqtFramerTestParams(message_type, true,
                                            quic::Perspective::IS_CLIENT));
    }
  }
  return params;
}

std::string ParamNameFormatter(
    const testing::TestParamInfo<MoqtFramerTestParams>& info) {
  return MoqtMessageTypeToString(info.param.message_type) + "_" +
         (info.param.uses_web_transport ? "WebTransport" : "QUIC") + "_" +
         quic::PerspectiveToString(info.param.perspective);
}

// If |change_in_object_id| is 0, it's the first object in the stream.
quiche::QuicheBuffer SerializeObject(MoqtFramer& framer,
                                     const MoqtObject& message,
                                     absl::string_view payload,
                                     MoqtDataStreamType stream_type,
                                     uint64_t change_in_object_id) {
  MoqtObject adjusted_message = message;
  adjusted_message.payload_length = payload.size();
  QUICHE_DCHECK(message.object_id > change_in_object_id);
  std::optional<PublishedObjectMetadata> previous_object;
  if (change_in_object_id > 0) {
    previous_object.emplace();
    previous_object->location =
        Location(message.group_id, message.object_id - change_in_object_id);
    previous_object->subgroup = message.subgroup_id;
    previous_object->properties = message.properties;
    previous_object->status = message.object_status;
    previous_object->publisher_priority = message.publisher_priority;
  }
  quiche::QuicheBuffer header = framer.SerializeObjectHeader(
      adjusted_message, stream_type, previous_object);
  if (header.empty()) {
    return quiche::QuicheBuffer();
  }
  return quiche::QuicheBuffer::Copy(
      quiche::SimpleBufferAllocator::Get(),
      absl::StrCat(header.AsStringView(), payload));
}

class MoqtFramerTest
    : public quic::test::QuicTestWithParam<MoqtFramerTestParams> {
 public:
  MoqtFramerTest()
      : message_type_(GetParam().message_type),
        webtrans_(GetParam().uses_web_transport),
        perspective_(GetParam().perspective),
        framer_(GetParam().uses_web_transport, GetParam().perspective) {}

  std::unique_ptr<TestMessageBase> MakeMessage(MoqtMessageType message_type) {
    return CreateTestMessage(message_type, webtrans_, perspective_);
  }

  quiche::QuicheBuffer SerializeMessage(
      TestMessageBase::MessageStructuredData& structured_data) {
    switch (message_type_) {
      case MoqtMessageType::kRequestOk: {
        auto data = std::get<MoqtRequestOk>(structured_data);
        return framer_.SerializeRequestOk(data);
      }
      case MoqtMessageType::kRequestError: {
        auto data = std::get<MoqtRequestError>(structured_data);
        return framer_.SerializeRequestError(data);
      }
      case MoqtMessageType::kSubscribe: {
        auto data = std::get<MoqtSubscribe>(structured_data);
        return framer_.SerializeSubscribe(data);
      }
      case MoqtMessageType::kSubscribeOk: {
        auto data = std::get<MoqtSubscribeOk>(structured_data);
        return framer_.SerializeSubscribeOk(data);
      }
      case MoqtMessageType::kRequestUpdate: {
        auto data = std::get<MoqtRequestUpdate>(structured_data);
        return framer_.SerializeRequestUpdate(data);
      }
      case MoqtMessageType::kPublishDone: {
        auto data = std::get<MoqtPublishDone>(structured_data);
        return framer_.SerializePublishDone(data);
      }
      case MoqtMessageType::kPublishNamespace: {
        auto data = std::get<MoqtPublishNamespace>(structured_data);
        return framer_.SerializePublishNamespace(data);
      }
      case MoqtMessageType::kNamespace: {
        auto data = std::get<MoqtNamespace>(structured_data);
        return framer_.SerializeNamespace(data);
      }
      case MoqtMessageType::kNamespaceDone: {
        auto data = std::get<MoqtNamespaceDone>(structured_data);
        return framer_.SerializeNamespaceDone(data);
      }
      case MoqtMessageType::kPublishSkipped: {
        auto data = std::get<MoqtPublishSkipped>(structured_data);
        return framer_.SerializePublishSkipped(data);
      }
      case moqt::MoqtMessageType::kTrackStatus: {
        auto data = std::get<MoqtTrackStatus>(structured_data);
        return framer_.SerializeTrackStatus(data);
      }
      case moqt::MoqtMessageType::kGoAway: {
        auto data = std::get<MoqtGoAway>(structured_data);
        return framer_.SerializeGoAway(data);
      }
      case moqt::MoqtMessageType::kSubscribeNamespace: {
        auto data = std::get<MoqtSubscribeNamespace>(structured_data);
        return framer_.SerializeSubscribeNamespace(data);
      }
      case moqt::MoqtMessageType::kSubscribeTracks: {
        auto data = std::get<MoqtSubscribeTracks>(structured_data);
        return framer_.SerializeSubscribeTracks(data);
      }
      case moqt::MoqtMessageType::kFetch: {
        auto data = std::get<MoqtFetch>(structured_data);
        return framer_.SerializeFetch(data);
      }
      case moqt::MoqtMessageType::kFetchOk: {
        auto data = std::get<MoqtFetchOk>(structured_data);
        return framer_.SerializeFetchOk(data);
      }
      case moqt::MoqtMessageType::kPublish: {
        auto data = std::get<MoqtPublish>(structured_data);
        return framer_.SerializePublish(data);
      }
      case moqt::MoqtMessageType::kPublishStateNotify: {
        auto data = std::get<MoqtPublishStateNotify>(structured_data);
        return framer_.SerializePublishStateNotify(data);
      }
      case moqt::MoqtMessageType::kObjectAck: {
        auto data = std::get<MoqtObjectAck>(structured_data);
        return framer_.SerializeObjectAck(data);
      }
      case MoqtMessageType::kSetup: {
        auto data = std::get<MoqtSetup>(structured_data);
        return framer_.SerializeSetup(data);
      }
      default:
        // kObjectDatagram is a totally different code path.
        return quiche::QuicheBuffer();
    }
  }

  MoqtMessageType message_type_;
  bool webtrans_;
  quic::Perspective perspective_;
  MoqtFramer framer_;
};

INSTANTIATE_TEST_SUITE_P(MoqtFramerTests, MoqtFramerTest,
                         testing::ValuesIn(GetMoqtFramerTestParams()),
                         ParamNameFormatter);

TEST_P(MoqtFramerTest, OneMessage) {
  auto message = MakeMessage(message_type_);
  auto structured_data = message->structured_data();
  auto buffer = SerializeMessage(structured_data);
  EXPECT_EQ(buffer.size(), message->total_message_size());
  quiche::test::CompareCharArraysWithHexError(
      "frame encoding", buffer.data(), buffer.size(),
      message->PacketSample().data(), message->PacketSample().size());
}

class MoqtFramerSimpleTest : public quic::test::QuicTest {
 public:
  MoqtFramerSimpleTest()
      : framer_(/*web_transport=*/true,
                /*perspective=*/quic::Perspective::IS_SERVER) {}

  MoqtFramer framer_;
  // Obtain a pointer to an arbitrary offset in a serialized buffer.
  const uint8_t* BufferAtOffset(quiche::QuicheBuffer& buffer, size_t offset) {
    const char* data = buffer.data();
    return reinterpret_cast<const uint8_t*>(data + offset);
  }
};

TEST_F(MoqtFramerSimpleTest, GroupMiddler) {
  MoqtDataStreamType type =
      MoqtDataStreamType::Subgroup(1, 1, true, false, true);
  auto header = std::make_unique<StreamHeaderSubgroupMessage>(type);
  auto buffer1 = SerializeObject(
      framer_, std::get<MoqtObject>(header->structured_data()), "foo", type, 0);
  EXPECT_EQ(buffer1.size(), header->total_message_size());
  EXPECT_EQ(buffer1.AsStringView(), header->PacketSample());

  auto middler = std::make_unique<StreamMiddlerSubgroupMessage>(type);
  auto buffer2 =
      SerializeObject(framer_, std::get<MoqtObject>(middler->structured_data()),
                      "bar", type, /*change_in_object_id=*/3);
  EXPECT_EQ(buffer2.size(), middler->total_message_size());
  EXPECT_EQ(buffer2.AsStringView(), middler->PacketSample());
}

TEST_F(MoqtFramerSimpleTest, FetchMiddler) {
  for (const MoqtFetchSerialization& flags : AllMoqtFetchSerializations()) {
    SCOPED_TRACE(testing::Message() << "flags: " << flags.value());
    if (flags.is_datagram() && !flags.zero_subgroup_id()) {
      // The framer will not encode these, although they are legal.
      continue;
    }
    auto header = std::make_unique<StreamHeaderFetchMessage>();
    MoqtObject object = std::get<MoqtObject>(header->structured_data());
    std::optional<PublishedObjectMetadata> previous;
    auto buffer1 = framer_.SerializeObjectHeader(
        object, MoqtDataStreamType::Fetch(), previous);
    auto whole_object =
        quiche::QuicheBuffer::Copy(quiche::SimpleBufferAllocator::Get(),
                                   absl::StrCat(buffer1.AsStringView(), "foo"));
    EXPECT_EQ(whole_object.size(), header->total_message_size());
    EXPECT_EQ(whole_object.AsStringView(), header->PacketSample());

    auto middler = std::make_unique<StreamMiddlerFetchMessage>(flags);
    // Populate previous object metadata.
    previous.emplace(Location(object.group_id, object.object_id),
                     object.subgroup_id, object.properties,
                     object.object_status, object.publisher_priority);
    auto buffer2 = framer_.SerializeObjectHeader(
        std::get<MoqtObject>(middler->structured_data()),
        MoqtDataStreamType::Fetch(), previous);
    whole_object =
        quiche::QuicheBuffer::Copy(quiche::SimpleBufferAllocator::Get(),
                                   absl::StrCat(buffer2.AsStringView(), "bar"));
    EXPECT_EQ(whole_object.size(), middler->total_message_size());
    EXPECT_EQ(whole_object.AsStringView(), middler->PacketSample());
  }
}

TEST_F(MoqtFramerSimpleTest, BadObjectInput) {
  MoqtObject object = {
      // Invalid: EndOfTrack with non-zero payload length.
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultPropertyBlob.data(), kDefaultPropertyBlob.size()),
      /*object_status=*/MoqtObjectStatus::kEndOfTrack,
      /*subgroup_id=*/8,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;
  std::optional<PublishedObjectMetadata> previous;
  EXPECT_QUIC_BUG(
      buffer = framer_.SerializeObjectHeader(
          object, MoqtDataStreamType::Subgroup(8, 0, false, false, true),
          previous),
      "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // kEndOfTrack with empty payload and non-empty properties is still invalid,
  // even when EndOfGroupInStream is set.
  object.payload_length = 0;
  EXPECT_QUIC_BUG(
      buffer = framer_.SerializeObjectHeader(
          object, MoqtDataStreamType::Subgroup(8, 0, false, false, true, true),
          previous),
      "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // kEndOfGroup with payload or properties is disallowed unless
  // EndOfGroupInStream is set.
  object.object_status = MoqtObjectStatus::kEndOfGroup;
  EXPECT_QUIC_BUG(
      buffer = framer_.SerializeObjectHeader(
          object, MoqtDataStreamType::Subgroup(8, 0, false, false, true, false),
          previous),
      "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // If EndOfGroupInStream is set, kEndOfGroup is allowed with non-empty
  // payload and properties.
  buffer = framer_.SerializeObjectHeader(
      object, MoqtDataStreamType::Subgroup(8, 0, false, false, true, true),
      previous);
  EXPECT_FALSE(buffer.empty());
  previous.reset();
  object.payload_length = 3;
  buffer = framer_.SerializeObjectHeader(
      object, MoqtDataStreamType::Subgroup(8, 0, false, false, true, true),
      previous);
  EXPECT_FALSE(buffer.empty());
}

TEST_F(MoqtFramerSimpleTest, BadDatagramInput) {
  MoqtObject object = {
      // This is a valid datagram.
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultPropertyBlob),
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/std::nullopt,
      /*first_object_in_subgroup=*/std::nullopt,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;

  object.object_status = MoqtObjectStatus::kEndOfTrack;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "foo", kDefaultPublisherPriority),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // kEndOfTrack with empty payload and non-empty properties is invalid.
  object.payload_length = 0;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "", kDefaultPublisherPriority),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // kEndOfGroup with empty payload and non-empty properties is valid via the
  // datagram kEndOfGroup flag.
  object.object_status = MoqtObjectStatus::kEndOfGroup;
  buffer =
      framer_.SerializeObjectDatagram(object, "", kDefaultPublisherPriority);
  EXPECT_FALSE(buffer.empty());
  object.payload_length = 3;
  object.object_status = MoqtObjectStatus::kNormal;

  object.subgroup_id = 8;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "foo", kDefaultPublisherPriority),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = std::nullopt;

  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "foobar", kDefaultPublisherPriority),
                  "Payload length does not match payload");
  EXPECT_TRUE(buffer.empty());

  // Range indicators cannot be sent in datagrams.
  object.properties = "";
  object.payload_length = 0;
  object.object_status = MoqtObjectStatus::kEndOfNonExistentRange;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "", kDefaultPublisherPriority),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
}

TEST_F(MoqtFramerSimpleTest, FetchEndOfRangeSerialization) {
  MoqtObject object = {
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/7,
      /*publisher_priority=*/0,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kEndOfNonExistentRange,
      /*subgroup_id=*/std::nullopt,
      /*first_object_in_subgroup=*/std::nullopt,
      /*payload_length=*/0,
  };
  quiche::QuicheBuffer buffer;
  std::optional<PublishedObjectMetadata> previous;

  // Range indicators are disallowed on Subgroup streams.
  object.subgroup_id = 0;
  EXPECT_QUIC_BUG(
      buffer = framer_.SerializeObjectHeader(
          object, MoqtDataStreamType::Subgroup(0, 0, true, false, true),
          previous),
      "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = std::nullopt;

  // Range indicators cannot have non-empty payload or properties even in FETCH.
  object.payload_length = 3;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectHeader(
                      object, MoqtDataStreamType::Fetch(), previous),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.payload_length = 0;

  // First item in FETCH: 0x05 (Fetch), 0x04 (id), 0x808c, group 5, object 7.
  buffer = framer_.SerializeObjectHeader(object, MoqtDataStreamType::Fetch(),
                                         previous);
  constexpr absl::string_view kExpectedFirst("\x05\x04\x80\x8c\x05\x07", 6);
  EXPECT_EQ(buffer.AsStringView(), kExpectedFirst);

  // Second item in FETCH: kEndOfUnknownRange at (group 8, object 2) ->
  // 0x810c, group delta = 8 - 5 - 1 = 2, explicit object ID = 2.
  previous.emplace();
  previous->location = Location(5, 7);
  object.group_id = 8;
  object.object_id = 2;
  object.object_status = MoqtObjectStatus::kEndOfUnknownRange;
  buffer = framer_.SerializeObjectHeader(object, MoqtDataStreamType::Fetch(),
                                         previous);
  constexpr absl::string_view kExpectedSecond("\x81\x0c\x02\x02", 4);
  EXPECT_EQ(buffer.AsStringView(), kExpectedSecond);
}

TEST_F(MoqtFramerSimpleTest, AllDatagramTypes) {
  for (MoqtDatagramType type : AllMoqtDatagramTypes()) {
    ObjectDatagramMessage message(type);
    MoqtObject object = std::get<MoqtObject>(message.structured_data());
    quiche::QuicheBuffer buffer = framer_.SerializeObjectDatagram(
        object, type.has_status() ? "" : "foo",
        type.has_default_priority() ? object.publisher_priority
                                    : (object.publisher_priority + 1));
    EXPECT_EQ(buffer.size(), message.total_message_size());
    EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
  }
}

TEST_F(MoqtFramerSimpleTest, AllSubscribeInputs) {
  for (auto filter : {MoqtLocationFilterType::kNoFilter,
                      MoqtLocationFilterType::kRelativeStart,
                      MoqtLocationFilterType::kAbsoluteStart,
                      MoqtLocationFilterType::kAbsoluteStartGroupEnd,
                      MoqtLocationFilterType::kAbsoluteRange,
                      MoqtLocationFilterType::kNextObject}) {
    MessageParameters parameters = SubscribeForTest();
    switch (filter) {
      case MoqtLocationFilterType::kNoFilter:
        parameters.location_filter = LocationFilter::NoLocationFilter();
        break;
      case MoqtLocationFilterType::kRelativeStart:
        parameters.location_filter = LocationFilter::RelativeStartFilter(2);
        break;
      case MoqtLocationFilterType::kAbsoluteStart:
        parameters.location_filter =
            LocationFilter::AbsoluteStartFilter(Location(4, 3));
        break;
      case MoqtLocationFilterType::kAbsoluteStartGroupEnd:
        parameters.location_filter =
            LocationFilter::AbsoluteStartGroupEndFilter(Location(4, 3), 5);
        break;
      case MoqtLocationFilterType::kAbsoluteRange:
        parameters.location_filter =
            LocationFilter::AbsoluteRangeFilter(Location(4, 3), Location(5, 2));
        break;
      case MoqtLocationFilterType::kNextObject:
        parameters.location_filter = LocationFilter::NextObjectFilter();
        break;
    }
    MoqtSubscribe subscribe = {/*request_id=*/3, FullTrackName({"foo", "abcd"}),
                               parameters};
    quiche::QuicheBuffer buffer;
    buffer = framer_.SerializeSubscribe(subscribe);
    EXPECT_GT(buffer.size(), 0);
  }
}

TEST_F(MoqtFramerSimpleTest, FetchEndBeforeStart) {
  MoqtFetch fetch = {
      /*request_id=*/1,
      StandaloneFetch{
          FullTrackName("foo", "bar"),
          /*start_location=*/Location{1, 2},
          /*end_location=*/Location{1, 1},
      },
      MessageParameters(),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeFetch(fetch),
                  "Invalid FETCH object range");
  EXPECT_EQ(buffer.size(), 0);
  std::get<StandaloneFetch>(fetch.fetch).end_location =
      Location{0, kMaxObjectId};
  EXPECT_QUIC_BUG(buffer = framer_.SerializeFetch(fetch),
                  "Invalid FETCH object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, FetchOkWholeGroup) {
  MoqtFetchOk fetch_ok = {
      /*end_of_track=*/false,
      /*end_location=*/Location{4, kMaxObjectId},
      MessageParameters(),
      TrackProperties(),
  };
  quiche::QuicheBuffer buffer = framer_.SerializeFetchOk(fetch_ok);
  // Check that object ID is zero.
  EXPECT_EQ(static_cast<uint8_t>(buffer.AsSpan()[5]), 0);
}

TEST_F(MoqtFramerSimpleTest, RelativeJoiningFetch) {
  RelativeJoiningFetchMessage message;
  MoqtFetch fetch = std::get<MoqtFetch>(message.structured_data());
  quiche::QuicheBuffer buffer = framer_.SerializeFetch(fetch);
  EXPECT_EQ(buffer.size(), message.total_message_size());
  EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
}

TEST_F(MoqtFramerSimpleTest, AbsoluteJoiningFetch) {
  AbsoluteJoiningFetchMessage message;
  MoqtFetch fetch = std::get<MoqtFetch>(message.structured_data());
  quiche::QuicheBuffer buffer = framer_.SerializeFetch(fetch);
  EXPECT_EQ(buffer.size(), message.total_message_size());
  EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
}

TEST_F(MoqtFramerSimpleTest, GoAwayNewSessionUriTooLong) {
  MoqtGoAway goaway = {
      /*new_session_uri=*/std::string(kMaxNewSessionUriLength, 'a'),
      /*timeout=*/quic::QuicTimeDelta::FromMilliseconds(3),
  };
  quiche::QuicheBuffer buffer = framer_.SerializeGoAway(goaway);
  EXPECT_GT(buffer.size(), 0);

  goaway.new_session_uri = std::string(kMaxNewSessionUriLength + 1, 'a');
  EXPECT_QUIC_BUG(buffer = framer_.SerializeGoAway(goaway),
                  "New session URI is too long");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, GoAwayNewSessionUriFromClient) {
  MoqtFramer client_framer(/*using_webtrans=*/true,
                           quic::Perspective::IS_CLIENT);
  MoqtGoAway goaway = {
      /*new_session_uri=*/"",
      /*timeout=*/quic::QuicTimeDelta::FromMilliseconds(3),
  };
  quiche::QuicheBuffer buffer = client_framer.SerializeGoAway(goaway);
  EXPECT_GT(buffer.size(), 0);

  goaway.new_session_uri = "foo";
  EXPECT_QUIC_BUG(buffer = client_framer.SerializeGoAway(goaway),
                  "New session URI must be empty from client");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, MessageParametersTimeDeltaSerialization) {
  MessageParameters params;
  params.object_delivery_timeout = quic::QuicTimeDelta::Infinite();
  params.subgroup_delivery_timeout = quic::QuicTimeDelta::FromMicroseconds(500);
  params.expires = quic::QuicTimeDelta::Infinite();

  quiche::QuicheBuffer buffer =
      framer_.SerializeRequestOk(MoqtRequestOk{params, TrackProperties()});
  const uint8_t expected_infinite_expires[] = {
      0x07,        // MoqtMessageType::kRequestOk
      0x00, 0x07,  // Payload length = 7
      0x03,        // 3 parameters
      0x02, 0x00,  // kObjectDeliveryTimeout (0x02): Infinite -> 0
      0x04, 0x01,  // kSubgroupDeliveryTimeout (+0x04 = 0x06): 500us -> 1ms
      0x02, 0x00,  // kExpires (+0x02 = 0x08): Infinite -> 0
  };
  quiche::test::CompareCharArraysWithHexError(
      "frame encoding", buffer.data(), buffer.size(),
      reinterpret_cast<const char*>(expected_infinite_expires),
      sizeof(expected_infinite_expires));

  params.expires = quic::QuicTimeDelta::FromMicroseconds(200);
  buffer = framer_.SerializeRequestOk(MoqtRequestOk{params, TrackProperties()});
  const uint8_t expected_clamped_expires[] = {
      0x07,        // MoqtMessageType::kRequestOk
      0x00, 0x07,  // Payload length = 7
      0x03,        // 3 parameters
      0x02, 0x00,  // kObjectDeliveryTimeout (0x02): Infinite -> 0
      0x04, 0x01,  // kSubgroupDeliveryTimeout (+0x04 = 0x06): 500us -> 1ms
      0x02, 0x01,  // kExpires (+0x02 = 0x08): 200us -> 1ms
  };
  quiche::test::CompareCharArraysWithHexError(
      "frame encoding", buffer.data(), buffer.size(),
      reinterpret_cast<const char*>(expected_clamped_expires),
      sizeof(expected_clamped_expires));
}

TEST_F(MoqtFramerSimpleTest, RequestErrorRedirect) {
  MoqtRequestError error = {
      RequestErrorCode::kRedirect,
      std::nullopt,
      "bar",
      Redirect{"uri", FullTrackName("foo", "abcd")},
  };
  quiche::QuicheBuffer buffer = framer_.SerializeRequestError(error);
  const uint8_t expected[] = {
      0x05, 0x00, 0x14,
      0x34,                          // error_code = kRedirect (0x34)
      0x00,                          // retry_interval = 0
      0x03, 0x62, 0x61, 0x72,        // reason_phrase = "bar"
      0x03, 0x75, 0x72, 0x69,        // connect_uri = "uri"
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
  };
  EXPECT_EQ(buffer.size(), sizeof(expected));
  quiche::test::CompareCharArraysWithHexError(
      "frame encoding", buffer.data(), buffer.size(),
      reinterpret_cast<const char*>(expected), sizeof(expected));

  // Client cannot send non-empty connect_uri.
  MoqtFramer client_framer(/*using_webtrans=*/true,
                           quic::Perspective::IS_CLIENT);
  EXPECT_QUIC_BUG(buffer = client_framer.SerializeRequestError(error),
                  "Connect URI must be empty from client");
  EXPECT_EQ(buffer.size(), 0);

  error.redirect->connect_uri = "";
  buffer = client_framer.SerializeRequestError(error);
  EXPECT_GT(buffer.size(), 0);

  // kRedirect without redirect struct fails.
  error.redirect = std::nullopt;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeRequestError(error),
                  "Redirect presence must match kRedirect error code");
  EXPECT_EQ(buffer.size(), 0);

  // Non-kRedirect with redirect struct fails.
  error.error_code = RequestErrorCode::kInternalError;
  error.redirect = Redirect{"", FullTrackName()};
  EXPECT_QUIC_BUG(buffer = framer_.SerializeRequestError(error),
                  "Redirect presence must match kRedirect error code");
  EXPECT_EQ(buffer.size(), 0);
}

}  // namespace moqt::test
