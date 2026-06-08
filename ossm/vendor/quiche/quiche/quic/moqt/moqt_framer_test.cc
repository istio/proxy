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
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_test_message.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {

struct MoqtFramerTestParams {
  MoqtFramerTestParams(MoqtMessageType message_type, bool uses_web_transport)
      : message_type(message_type), uses_web_transport(uses_web_transport) {}
  MoqtMessageType message_type;
  bool uses_web_transport;
};

std::vector<MoqtFramerTestParams> GetMoqtFramerTestParams() {
  std::vector<MoqtFramerTestParams> params;
  std::vector<MoqtMessageType> message_types = {
      MoqtMessageType::kRequestOk,
      MoqtMessageType::kRequestError,
      MoqtMessageType::kSubscribe,
      MoqtMessageType::kSubscribeOk,
      MoqtMessageType::kUnsubscribe,
      MoqtMessageType::kPublishDone,
      MoqtMessageType::kPublishNamespace,
      MoqtMessageType::kPublishNamespaceDone,
      MoqtMessageType::kNamespace,
      MoqtMessageType::kNamespaceDone,
      MoqtMessageType::kPublishNamespaceCancel,
      MoqtMessageType::kTrackStatus,
      MoqtMessageType::kGoAway,
      MoqtMessageType::kSubscribeNamespace,
      MoqtMessageType::kMaxRequestId,
      MoqtMessageType::kFetch,
      MoqtMessageType::kFetchCancel,
      MoqtMessageType::kFetchOk,
      MoqtMessageType::kRequestsBlocked,
      MoqtMessageType::kPublish,
      MoqtMessageType::kPublishOk,
      MoqtMessageType::kObjectAck,
      MoqtMessageType::kClientSetup,
      MoqtMessageType::kServerSetup,
  };
  for (const MoqtMessageType message_type : message_types) {
    if (message_type == MoqtMessageType::kClientSetup) {
      for (const bool uses_web_transport : {false, true}) {
        params.push_back(
            MoqtFramerTestParams(message_type, uses_web_transport));
      }
    } else {
      // All other types are processed the same for either perspective or
      // transport.
      params.push_back(MoqtFramerTestParams(message_type, true));
    }
  }
  return params;
}

std::string ParamNameFormatter(
    const testing::TestParamInfo<MoqtFramerTestParams>& info) {
  return MoqtMessageTypeToString(info.param.message_type) + "_" +
         (info.param.uses_web_transport ? "WebTransport" : "QUIC");
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
    previous_object->extensions = message.extension_headers;
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
        framer_(GetParam().uses_web_transport) {}

  std::unique_ptr<TestMessageBase> MakeMessage(MoqtMessageType message_type) {
    return CreateTestMessage(message_type, webtrans_);
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
      case MoqtMessageType::kUnsubscribe: {
        auto data = std::get<MoqtUnsubscribe>(structured_data);
        return framer_.SerializeUnsubscribe(data);
      }
      case MoqtMessageType::kPublishDone: {
        auto data = std::get<MoqtPublishDone>(structured_data);
        return framer_.SerializePublishDone(data);
      }
      case MoqtMessageType::kPublishNamespace: {
        auto data = std::get<MoqtPublishNamespace>(structured_data);
        return framer_.SerializePublishNamespace(data);
      }
      case MoqtMessageType::kPublishNamespaceDone: {
        auto data = std::get<MoqtPublishNamespaceDone>(structured_data);
        return framer_.SerializePublishNamespaceDone(data);
      }
      case MoqtMessageType::kNamespace: {
        auto data = std::get<MoqtNamespace>(structured_data);
        return framer_.SerializeNamespace(data);
      }
      case MoqtMessageType::kNamespaceDone: {
        auto data = std::get<MoqtNamespaceDone>(structured_data);
        return framer_.SerializeNamespaceDone(data);
      }
      case moqt::MoqtMessageType::kPublishNamespaceCancel: {
        auto data = std::get<MoqtPublishNamespaceCancel>(structured_data);
        return framer_.SerializePublishNamespaceCancel(data);
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
      case moqt::MoqtMessageType::kMaxRequestId: {
        auto data = std::get<MoqtMaxRequestId>(structured_data);
        return framer_.SerializeMaxRequestId(data);
      }
      case moqt::MoqtMessageType::kFetch: {
        auto data = std::get<MoqtFetch>(structured_data);
        return framer_.SerializeFetch(data);
      }
      case moqt::MoqtMessageType::kFetchCancel: {
        auto data = std::get<MoqtFetchCancel>(structured_data);
        return framer_.SerializeFetchCancel(data);
      }
      case moqt::MoqtMessageType::kFetchOk: {
        auto data = std::get<MoqtFetchOk>(structured_data);
        return framer_.SerializeFetchOk(data);
      }
      case moqt::MoqtMessageType::kRequestsBlocked: {
        auto data = std::get<MoqtRequestsBlocked>(structured_data);
        return framer_.SerializeRequestsBlocked(data);
      }
      case moqt::MoqtMessageType::kPublish: {
        auto data = std::get<MoqtPublish>(structured_data);
        return framer_.SerializePublish(data);
      }
      case moqt::MoqtMessageType::kPublishOk: {
        auto data = std::get<MoqtPublishOk>(structured_data);
        return framer_.SerializePublishOk(data);
      }
      case moqt::MoqtMessageType::kObjectAck: {
        auto data = std::get<MoqtObjectAck>(structured_data);
        return framer_.SerializeObjectAck(data);
      }
      case MoqtMessageType::kClientSetup: {
        auto data = std::get<MoqtClientSetup>(structured_data);
        return framer_.SerializeClientSetup(data);
      }
      case MoqtMessageType::kServerSetup: {
        auto data = std::get<MoqtServerSetup>(structured_data);
        return framer_.SerializeServerSetup(data);
      }
      default:
        // kObjectDatagram is a totally different code path.
        return quiche::QuicheBuffer();
    }
  }

  MoqtMessageType message_type_;
  bool webtrans_;
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
  MoqtFramerSimpleTest() : framer_(/*web_transport=*/true) {}

  MoqtFramer framer_;
  // Obtain a pointer to an arbitrary offset in a serialized buffer.
  const uint8_t* BufferAtOffset(quiche::QuicheBuffer& buffer, size_t offset) {
    const char* data = buffer.data();
    return reinterpret_cast<const uint8_t*>(data + offset);
  }
};

TEST_F(MoqtFramerSimpleTest, GroupMiddler) {
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, true, false);
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
                     object.subgroup_id, object.extension_headers,
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
      // Invalid: DoesNotExist with non-zero payload length.
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultExtensionBlob.data(), kDefaultExtensionBlob.size()),
      /*object_status=*/MoqtObjectStatus::kObjectDoesNotExist,
      /*subgroup_id=*/8,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;
  std::optional<PublishedObjectMetadata> previous;
  EXPECT_QUIC_BUG(
      buffer = framer_.SerializeObjectHeader(
          object, MoqtDataStreamType::Subgroup(8, 0, false, false), previous),
      "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
}

TEST_F(MoqtFramerSimpleTest, BadDatagramInput) {
  MoqtObject object = {
      // This is a valid datagram.
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultExtensionBlob),
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/std::nullopt,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;

  object.object_status = MoqtObjectStatus::kObjectDoesNotExist;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(
                      object, "foo", kDefaultPublisherPriority),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
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
  for (auto filter :
       {MoqtFilterType::kNextGroupStart, MoqtFilterType::kLargestObject,
        MoqtFilterType::kAbsoluteStart, MoqtFilterType::kAbsoluteRange}) {
    MessageParameters parameters = SubscribeForTest();
    switch (filter) {
      case MoqtFilterType::kAbsoluteRange:
        parameters.subscription_filter.emplace(Location(4, 3));
        break;
      case MoqtFilterType::kAbsoluteStart:
        parameters.subscription_filter.emplace(Location(4, 3), 5);
        break;
      default:
        parameters.subscription_filter.emplace(filter);
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
      /*request_id=*/1,
      /*end_of_track=*/false,
      /*end_location=*/Location{4, kMaxObjectId},
      MessageParameters(),
      TrackExtensions(),
  };
  quiche::QuicheBuffer buffer = framer_.SerializeFetchOk(fetch_ok);
  // Check that object ID is zero.
  EXPECT_EQ(static_cast<uint8_t>(buffer.AsSpan()[7]), 0);
}

TEST_F(MoqtFramerSimpleTest, RelativeJoiningFetch) {
  RelativeJoiningFetchMessage message;
  quiche::QuicheBuffer buffer =
      framer_.SerializeFetch(std::get<MoqtFetch>(message.structured_data()));
  EXPECT_EQ(buffer.size(), message.total_message_size());
  EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
}

TEST_F(MoqtFramerSimpleTest, AbsoluteJoiningFetch) {
  AbsoluteJoiningFetchMessage message;
  quiche::QuicheBuffer buffer =
      framer_.SerializeFetch(std::get<MoqtFetch>(message.structured_data()));
  EXPECT_EQ(buffer.size(), message.total_message_size());
  EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
}

}  // namespace moqt::test
