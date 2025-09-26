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
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_priority.h"
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
      MoqtMessageType::kSubscribe,
      MoqtMessageType::kSubscribeOk,
      MoqtMessageType::kSubscribeError,
      MoqtMessageType::kUnsubscribe,
      MoqtMessageType::kPublishDone,
      MoqtMessageType::kPublishNamespace,
      MoqtMessageType::kPublishNamespaceOk,
      MoqtMessageType::kPublishNamespaceError,
      MoqtMessageType::kPublishNamespaceDone,
      MoqtMessageType::kPublishNamespaceCancel,
      MoqtMessageType::kTrackStatus,
      MoqtMessageType::kTrackStatusOk,
      MoqtMessageType::kTrackStatusError,
      MoqtMessageType::kGoAway,
      MoqtMessageType::kSubscribeNamespace,
      MoqtMessageType::kSubscribeNamespaceOk,
      MoqtMessageType::kSubscribeNamespaceError,
      MoqtMessageType::kUnsubscribeNamespace,
      MoqtMessageType::kMaxRequestId,
      MoqtMessageType::kFetch,
      MoqtMessageType::kFetchCancel,
      MoqtMessageType::kFetchOk,
      MoqtMessageType::kFetchError,
      MoqtMessageType::kRequestsBlocked,
      MoqtMessageType::kPublish,
      MoqtMessageType::kPublishOk,
      MoqtMessageType::kPublishError,
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
  quiche::QuicheBuffer header = framer.SerializeObjectHeader(
      adjusted_message, stream_type,
      change_in_object_id == 0
          ? std::nullopt
          : std::optional<uint64_t>(message.object_id - change_in_object_id));
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
        buffer_allocator_(quiche::SimpleBufferAllocator::Get()),
        framer_(buffer_allocator_, GetParam().uses_web_transport) {}

  std::unique_ptr<TestMessageBase> MakeMessage(MoqtMessageType message_type) {
    return CreateTestMessage(message_type, webtrans_);
  }

  quiche::QuicheBuffer SerializeMessage(
      TestMessageBase::MessageStructuredData& structured_data) {
    switch (message_type_) {
      case MoqtMessageType::kSubscribe: {
        auto data = std::get<MoqtSubscribe>(structured_data);
        return framer_.SerializeSubscribe(data);
      }
      case MoqtMessageType::kSubscribeOk: {
        auto data = std::get<MoqtSubscribeOk>(structured_data);
        return framer_.SerializeSubscribeOk(data);
      }
      case MoqtMessageType::kSubscribeError: {
        auto data = std::get<MoqtSubscribeError>(structured_data);
        return framer_.SerializeSubscribeError(data);
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
      case moqt::MoqtMessageType::kPublishNamespaceOk: {
        auto data = std::get<MoqtPublishNamespaceOk>(structured_data);
        return framer_.SerializePublishNamespaceOk(data);
      }
      case moqt::MoqtMessageType::kPublishNamespaceError: {
        auto data = std::get<MoqtPublishNamespaceError>(structured_data);
        return framer_.SerializePublishNamespaceError(data);
      }
      case MoqtMessageType::kPublishNamespaceDone: {
        auto data = std::get<MoqtPublishNamespaceDone>(structured_data);
        return framer_.SerializePublishNamespaceDone(data);
      }
      case moqt::MoqtMessageType::kPublishNamespaceCancel: {
        auto data = std::get<MoqtPublishNamespaceCancel>(structured_data);
        return framer_.SerializePublishNamespaceCancel(data);
      }
      case moqt::MoqtMessageType::kTrackStatus: {
        auto data = std::get<MoqtTrackStatus>(structured_data);
        return framer_.SerializeTrackStatus(data);
      }
      case moqt::MoqtMessageType::kTrackStatusOk: {
        auto data = std::get<MoqtTrackStatusOk>(structured_data);
        return framer_.SerializeTrackStatusOk(data);
      }
      case moqt::MoqtMessageType::kTrackStatusError: {
        auto data = std::get<MoqtTrackStatusError>(structured_data);
        return framer_.SerializeTrackStatusError(data);
      }
      case moqt::MoqtMessageType::kGoAway: {
        auto data = std::get<MoqtGoAway>(structured_data);
        return framer_.SerializeGoAway(data);
      }
      case moqt::MoqtMessageType::kSubscribeNamespace: {
        auto data = std::get<MoqtSubscribeNamespace>(structured_data);
        return framer_.SerializeSubscribeNamespace(data);
      }
      case moqt::MoqtMessageType::kSubscribeNamespaceOk: {
        auto data = std::get<MoqtSubscribeNamespaceOk>(structured_data);
        return framer_.SerializeSubscribeNamespaceOk(data);
      }
      case moqt::MoqtMessageType::kSubscribeNamespaceError: {
        auto data = std::get<MoqtSubscribeNamespaceError>(structured_data);
        return framer_.SerializeSubscribeNamespaceError(data);
      }
      case moqt::MoqtMessageType::kUnsubscribeNamespace: {
        auto data = std::get<MoqtUnsubscribeNamespace>(structured_data);
        return framer_.SerializeUnsubscribeNamespace(data);
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
      case moqt::MoqtMessageType::kFetchError: {
        auto data = std::get<MoqtFetchError>(structured_data);
        return framer_.SerializeFetchError(data);
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
      case moqt::MoqtMessageType::kPublishError: {
        auto data = std::get<MoqtPublishError>(structured_data);
        return framer_.SerializePublishError(data);
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
  quiche::SimpleBufferAllocator* buffer_allocator_;
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
      : buffer_allocator_(quiche::SimpleBufferAllocator::Get()),
        framer_(buffer_allocator_, /*web_transport=*/true) {}

  quiche::SimpleBufferAllocator* buffer_allocator_;
  MoqtFramer framer_;

  // Obtain a pointer to an arbitrary offset in a serialized buffer.
  const uint8_t* BufferAtOffset(quiche::QuicheBuffer& buffer, size_t offset) {
    const char* data = buffer.data();
    return reinterpret_cast<const uint8_t*>(data + offset);
  }
};

TEST_F(MoqtFramerSimpleTest, GroupMiddler) {
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, true);
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
  auto header = std::make_unique<StreamHeaderFetchMessage>();
  auto buffer1 =
      SerializeObject(framer_, std::get<MoqtObject>(header->structured_data()),
                      "foo", MoqtDataStreamType::Fetch(), 0);
  EXPECT_EQ(buffer1.size(), header->total_message_size());
  EXPECT_EQ(buffer1.AsStringView(), header->PacketSample());

  auto middler = std::make_unique<StreamMiddlerFetchMessage>();
  auto buffer2 =
      SerializeObject(framer_, std::get<MoqtObject>(middler->structured_data()),
                      "bar", MoqtDataStreamType::Fetch(), 3);
  EXPECT_EQ(buffer2.size(), middler->total_message_size());
  EXPECT_EQ(buffer2.AsStringView(), middler->PacketSample());
}

TEST_F(MoqtFramerSimpleTest, BadObjectInput) {
  MoqtObject object = {
      // This is a valid object.
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultExtensionBlob.data(), kDefaultExtensionBlob.size()),
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/8,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;

  // Non-normal status must have no payload.
  object.object_status = MoqtObjectStatus::kObjectDoesNotExist;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectHeader(
                      object, MoqtDataStreamType::Subgroup(8, 0, false), false),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  // object.object_status = MoqtObjectStatus::kNormal;
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
      /*subgroup_id=*/6,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;

  object.object_status = MoqtObjectStatus::kObjectDoesNotExist;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foo"),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.object_status = MoqtObjectStatus::kNormal;

  object.subgroup_id = 8;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foo"),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = 6;

  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foobar"),
                  "Payload length does not match payload");
  EXPECT_TRUE(buffer.empty());
}

TEST_F(MoqtFramerSimpleTest, AllDatagramTypes) {
  for (MoqtDatagramType type : AllMoqtDatagramTypes()) {
    ObjectDatagramMessage message(type);
    MoqtObject object = std::get<MoqtObject>(message.structured_data());
    quiche::QuicheBuffer buffer =
        framer_.SerializeObjectDatagram(object, type.has_status() ? "" : "foo");
    EXPECT_EQ(buffer.size(), message.total_message_size());
    EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
  }
}

TEST_F(MoqtFramerSimpleTest, AllSubscribeInputs) {
  for (auto filter :
       {MoqtFilterType::kNextGroupStart, MoqtFilterType::kLatestObject,
        MoqtFilterType::kAbsoluteStart, MoqtFilterType::kAbsoluteRange}) {
    MoqtSubscribe subscribe = {
        /*subscribe_id=*/3,
        /*full_track_name=*/FullTrackName({"foo", "abcd"}),
        /*subscriber_priority=*/0x20,
        /*group_order=*/std::nullopt,
        /*forward=*/true,
        /*filter_type=*/filter,
        /*start=*/std::make_optional<Location>(4, 1),
        /*end_group=*/std::make_optional<uint64_t>(6ULL),
        VersionSpecificParameters(AuthTokenType::kOutOfBand, "bar"),
    };
    quiche::QuicheBuffer buffer;
    buffer = framer_.SerializeSubscribe(subscribe);
    EXPECT_GT(buffer.size(), 0);
  }
}

TEST_F(MoqtFramerSimpleTest, SubscribeEndBeforeStart) {
  MoqtSubscribe subscribe = {
      /*subscribe_id=*/3,
      /*full_track_name=*/FullTrackName({"foo", "abcd"}),
      /*subscriber_priority=*/0x20,
      /*group_order=*/std::nullopt,
      /*forward=*/true,
      /*filter_type=*/MoqtFilterType::kAbsoluteRange,
      /*start=*/std::make_optional<Location>(4, 3),
      /*end_group=*/std::make_optional<uint64_t>(3ULL),
      VersionSpecificParameters(AuthTokenType::kOutOfBand, "bar"),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializeSubscribe(subscribe),
                    "Invalid object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, AbsoluteRangeStartMissing) {
  MoqtSubscribe subscribe = {
      /*subscribe_id=*/3,
      /*full_track_name=*/FullTrackName({"foo", "abcd"}),
      /*subscriber_priority=*/0x20,
      /*group_order=*/std::nullopt,
      /*forward=*/true,
      /*filter_type=*/MoqtFilterType::kAbsoluteRange,
      /*start=*/std::nullopt,
      /*end_group=*/std::make_optional<uint64_t>(3ULL),
      VersionSpecificParameters(AuthTokenType::kOutOfBand, "bar"),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializeSubscribe(subscribe),
                    "Invalid object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, AbsoluteRangeEndMissing) {
  MoqtSubscribe subscribe = {
      /*subscribe_id=*/3,
      /*full_track_name=*/FullTrackName({"foo", "abcd"}),
      /*subscriber_priority=*/0x20,
      /*group_order=*/std::nullopt,
      /*forward=*/true,
      /*filter_type=*/MoqtFilterType::kAbsoluteRange,
      /*start=*/std::make_optional<Location>(4, 3),
      /*end_group=*/std::nullopt,
      VersionSpecificParameters(AuthTokenType::kOutOfBand, "bar"),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializeSubscribe(subscribe),
                    "Invalid object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, PublishOkEndBeforeStart) {
  MoqtPublishOk publish_ok = {
      /*request_id=*/1,
      /*forward=*/true,
      /*subscriber_priority=*/2,
      /*group_order=*/MoqtDeliveryOrder::kAscending,
      /*filter_type=*/MoqtFilterType::kAbsoluteRange,
      /*start=*/Location{1, 2},
      /*end_group=*/0,
      /*parameters=*/VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializePublishOk(publish_ok),
                    "End group is less than start group");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, PublishOkMissingEndGroup) {
  MoqtPublishOk publish_ok = {
      /*request_id=*/1,
      /*forward=*/true,
      /*subscriber_priority=*/2,
      /*group_order=*/MoqtDeliveryOrder::kAscending,
      /*filter_type=*/MoqtFilterType::kAbsoluteRange,
      /*start=*/Location{1, 2},
      /*end_group=*/std::nullopt,
      /*parameters=*/VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializePublishOk(publish_ok),
                    "Serializing invalid MoQT filter type");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, PublishOkMissingStart) {
  MoqtPublishOk publish_ok = {
      /*request_id=*/1,
      /*forward=*/true,
      /*subscriber_priority=*/2,
      /*group_order=*/MoqtDeliveryOrder::kAscending,
      /*filter_type=*/MoqtFilterType::kAbsoluteStart,
      /*start=*/std::nullopt,
      /*end_group=*/std::nullopt,
      /*parameters=*/VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUICHE_BUG(buffer = framer_.SerializePublishOk(publish_ok),
                    "Serializing invalid MoQT filter type");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, FetchEndBeforeStart) {
  MoqtFetch fetch = {
      /*request_id=*/1,
      /*subscriber_priority=*/2,
      /*group_order=*/MoqtDeliveryOrder::kAscending,
      /*fetch=*/
      StandaloneFetch{
          FullTrackName("foo", "bar"),
          /*start_location=*/Location{1, 2},
          /*end_location=*/Location{1, 1},
      },
      /*parameters=*/
      VersionSpecificParameters(AuthTokenType::kOutOfBand, "baz"),
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
      MoqtDeliveryOrder::kAscending,
      /*end_of_track=*/false,
      /*end_location=*/Location{4, kMaxObjectId},
      VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer = framer_.SerializeFetchOk(fetch_ok);
  // Check that object ID is zero.
  EXPECT_EQ(static_cast<uint8_t>(buffer.AsSpan()[7]), 0);
}

TEST_F(MoqtFramerSimpleTest, SubscribeUpdateEndGroupOnly) {
  MoqtSubscribeUpdate subscribe_update = {
      /*subscribe_id=*/3,
      /*start=*/Location(4, 3),
      /*end_group=*/4,
      /*subscriber_priority=*/0xaa,
      /*forward=*/true,
      VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeSubscribeUpdate(subscribe_update);
  EXPECT_GT(buffer.size(), 0);
  const uint8_t* end_group = BufferAtOffset(buffer, 6);
  EXPECT_EQ(*end_group, 5);
}

TEST_F(MoqtFramerSimpleTest, SubscribeUpdateIncrementsEnd) {
  MoqtSubscribeUpdate subscribe_update = {
      /*subscribe_id=*/3,
      /*start=*/Location(4, 3),
      /*end_group=*/4,
      /*subscriber_priority=*/0xaa,
      /*forward=*/true,
      VersionSpecificParameters(),
  };
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeSubscribeUpdate(subscribe_update);
  EXPECT_GT(buffer.size(), 0);
  const uint8_t* end_group = BufferAtOffset(buffer, 6);
  EXPECT_EQ(*end_group, 5);
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
