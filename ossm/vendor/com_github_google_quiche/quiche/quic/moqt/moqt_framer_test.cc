// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_framer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
      MoqtMessageType::kSubscribeDone,
      MoqtMessageType::kAnnounceCancel,
      MoqtMessageType::kTrackStatusRequest,
      MoqtMessageType::kTrackStatus,
      MoqtMessageType::kAnnounce,
      MoqtMessageType::kAnnounceOk,
      MoqtMessageType::kAnnounceError,
      MoqtMessageType::kUnannounce,
      MoqtMessageType::kGoAway,
      MoqtMessageType::kSubscribeAnnounces,
      MoqtMessageType::kSubscribeAnnouncesOk,
      MoqtMessageType::kSubscribeAnnouncesError,
      MoqtMessageType::kUnsubscribeAnnounces,
      MoqtMessageType::kMaxSubscribeId,
      MoqtMessageType::kFetch,
      MoqtMessageType::kFetchCancel,
      MoqtMessageType::kFetchOk,
      MoqtMessageType::kFetchError,
      MoqtMessageType::kSubscribesBlocked,
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

quiche::QuicheBuffer SerializeObject(MoqtFramer& framer,
                                     const MoqtObject& message,
                                     absl::string_view payload,
                                     MoqtDataStreamType stream_type,
                                     bool is_first_in_stream) {
  MoqtObject adjusted_message = message;
  adjusted_message.payload_length = payload.size();
  quiche::QuicheBuffer header = framer.SerializeObjectHeader(
      adjusted_message, stream_type, is_first_in_stream);
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
      case MoqtMessageType::kSubscribeDone: {
        auto data = std::get<MoqtSubscribeDone>(structured_data);
        return framer_.SerializeSubscribeDone(data);
      }
      case MoqtMessageType::kAnnounce: {
        auto data = std::get<MoqtAnnounce>(structured_data);
        return framer_.SerializeAnnounce(data);
      }
      case moqt::MoqtMessageType::kAnnounceOk: {
        auto data = std::get<MoqtAnnounceOk>(structured_data);
        return framer_.SerializeAnnounceOk(data);
      }
      case moqt::MoqtMessageType::kAnnounceError: {
        auto data = std::get<MoqtAnnounceError>(structured_data);
        return framer_.SerializeAnnounceError(data);
      }
      case moqt::MoqtMessageType::kAnnounceCancel: {
        auto data = std::get<MoqtAnnounceCancel>(structured_data);
        return framer_.SerializeAnnounceCancel(data);
      }
      case moqt::MoqtMessageType::kTrackStatusRequest: {
        auto data = std::get<MoqtTrackStatusRequest>(structured_data);
        return framer_.SerializeTrackStatusRequest(data);
      }
      case MoqtMessageType::kUnannounce: {
        auto data = std::get<MoqtUnannounce>(structured_data);
        return framer_.SerializeUnannounce(data);
      }
      case moqt::MoqtMessageType::kTrackStatus: {
        auto data = std::get<MoqtTrackStatus>(structured_data);
        return framer_.SerializeTrackStatus(data);
      }
      case moqt::MoqtMessageType::kGoAway: {
        auto data = std::get<MoqtGoAway>(structured_data);
        return framer_.SerializeGoAway(data);
      }
      case moqt::MoqtMessageType::kSubscribeAnnounces: {
        auto data = std::get<MoqtSubscribeAnnounces>(structured_data);
        return framer_.SerializeSubscribeAnnounces(data);
      }
      case moqt::MoqtMessageType::kSubscribeAnnouncesOk: {
        auto data = std::get<MoqtSubscribeAnnouncesOk>(structured_data);
        return framer_.SerializeSubscribeAnnouncesOk(data);
      }
      case moqt::MoqtMessageType::kSubscribeAnnouncesError: {
        auto data = std::get<MoqtSubscribeAnnouncesError>(structured_data);
        return framer_.SerializeSubscribeAnnouncesError(data);
      }
      case moqt::MoqtMessageType::kUnsubscribeAnnounces: {
        auto data = std::get<MoqtUnsubscribeAnnounces>(structured_data);
        return framer_.SerializeUnsubscribeAnnounces(data);
      }
      case moqt::MoqtMessageType::kMaxSubscribeId: {
        auto data = std::get<MoqtMaxSubscribeId>(structured_data);
        return framer_.SerializeMaxSubscribeId(data);
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
      case moqt::MoqtMessageType::kSubscribesBlocked: {
        auto data = std::get<MoqtSubscribesBlocked>(structured_data);
        return framer_.SerializeSubscribesBlocked(data);
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
  auto header = std::make_unique<StreamHeaderSubgroupMessage>();
  auto buffer1 =
      SerializeObject(framer_, std::get<MoqtObject>(header->structured_data()),
                      "foo", MoqtDataStreamType::kStreamHeaderSubgroup, true);
  EXPECT_EQ(buffer1.size(), header->total_message_size());
  EXPECT_EQ(buffer1.AsStringView(), header->PacketSample());

  auto middler = std::make_unique<StreamMiddlerSubgroupMessage>();
  auto buffer2 =
      SerializeObject(framer_, std::get<MoqtObject>(middler->structured_data()),
                      "bar", MoqtDataStreamType::kStreamHeaderSubgroup, false);
  EXPECT_EQ(buffer2.size(), middler->total_message_size());
  EXPECT_EQ(buffer2.AsStringView(), middler->PacketSample());
}

TEST_F(MoqtFramerSimpleTest, FetchMiddler) {
  auto header = std::make_unique<StreamHeaderFetchMessage>();
  auto buffer1 =
      SerializeObject(framer_, std::get<MoqtObject>(header->structured_data()),
                      "foo", MoqtDataStreamType::kStreamHeaderFetch, true);
  EXPECT_EQ(buffer1.size(), header->total_message_size());
  EXPECT_EQ(buffer1.AsStringView(), header->PacketSample());

  auto middler = std::make_unique<StreamMiddlerFetchMessage>();
  auto buffer2 =
      SerializeObject(framer_, std::get<MoqtObject>(middler->structured_data()),
                      "bar", MoqtDataStreamType::kStreamHeaderFetch, false);
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

  // kSubgroup must have a subgroup_id.
  object.subgroup_id = std::nullopt;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectHeader(
                      object, MoqtDataStreamType::kStreamHeaderSubgroup, false),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = 8;

  // kFetch must have a subgroup_id.
  object.subgroup_id = std::nullopt;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectHeader(
                      object, MoqtDataStreamType::kStreamHeaderFetch, false),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = 8;

  // Non-normal status must have no payload.
  object.object_status = MoqtObjectStatus::kEndOfGroup;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectHeader(
                      object, MoqtDataStreamType::kStreamHeaderSubgroup, false),
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
      /*subgroup_id=*/std::nullopt,
      /*payload_length=*/3,
  };
  quiche::QuicheBuffer buffer;

  object.object_status = MoqtObjectStatus::kEndOfGroup;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foo"),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.object_status = MoqtObjectStatus::kNormal;

  object.subgroup_id = 8;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foo"),
                  "Object metadata is invalid");
  EXPECT_TRUE(buffer.empty());
  object.subgroup_id = std::nullopt;

  EXPECT_QUIC_BUG(buffer = framer_.SerializeObjectDatagram(object, "foobar"),
                  "Payload length does not match payload");
  EXPECT_TRUE(buffer.empty());
}

TEST_F(MoqtFramerSimpleTest, Datagram) {
  auto datagram = std::make_unique<ObjectDatagramMessage>();
  MoqtObject object = {
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultExtensionBlob),
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/std::nullopt,
      /*payload_length=*/3,
  };
  std::string payload = "foo";
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeObjectDatagram(object, payload);
  EXPECT_EQ(buffer.size(), datagram->total_message_size());
  EXPECT_EQ(buffer.AsStringView(), datagram->PacketSample());
}

TEST_F(MoqtFramerSimpleTest, DatagramStatus) {
  auto datagram = std::make_unique<ObjectStatusDatagramMessage>();
  MoqtObject object = {
      /*track_alias=*/4,
      /*group_id=*/5,
      /*object_id=*/6,
      /*publisher_priority=*/7,
      std::string(kDefaultExtensionBlob),
      /*object_status=*/MoqtObjectStatus::kEndOfGroup,
      /*subgroup_id=*/std::nullopt,
      /*payload_length=*/0,
  };
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeObjectDatagram(object, "");
  EXPECT_EQ(buffer.size(), datagram->total_message_size());
  EXPECT_EQ(buffer.AsStringView(), datagram->PacketSample());
}

TEST_F(MoqtFramerSimpleTest, AllSubscribeInputs) {
  for (std::optional<FullSequence> start :
       {std::optional<FullSequence>(),
        std::optional<FullSequence>(std::in_place, 4, 0)}) {
    for (std::optional<uint64_t> end_group :
         {std::optional<uint64_t>(), std::optional<uint64_t>(7)}) {
      MoqtSubscribe subscribe = {
          /*subscribe_id=*/3,
          /*track_alias=*/4,
          /*full_track_name=*/FullTrackName({"foo", "abcd"}),
          /*subscriber_priority=*/0x20,
          /*group_order=*/std::nullopt,
          start,
          end_group,
          MoqtSubscribeParameters{"bar", std::nullopt, std::nullopt,
                                  std::nullopt},
      };
      quiche::QuicheBuffer buffer;
      MoqtFilterType expected_filter_type = GetFilterType(subscribe);
      if (expected_filter_type == MoqtFilterType::kNone) {
        EXPECT_QUIC_BUG(buffer = framer_.SerializeSubscribe(subscribe),
                        "Invalid object range");
        EXPECT_EQ(buffer.size(), 0);
        continue;
      }
      buffer = framer_.SerializeSubscribe(subscribe);
      // Go to the filter type.
      const uint8_t* read = BufferAtOffset(buffer, 16);
      EXPECT_EQ(static_cast<MoqtFilterType>(*read), expected_filter_type);
      EXPECT_GT(buffer.size(), 0);
    }
  }
}

TEST_F(MoqtFramerSimpleTest, SubscribeEndBeforeStart) {
  MoqtSubscribe subscribe = {
      /*subscribe_id=*/3,
      /*track_alias=*/4,
      /*full_track_name=*/FullTrackName({"foo", "abcd"}),
      /*subscriber_priority=*/0x20,
      /*group_order=*/std::nullopt,
      /*start=*/FullSequence(4, 3),
      /*end_group=*/3,
      MoqtSubscribeParameters{"bar", std::nullopt, std::nullopt, std::nullopt},
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeSubscribe(subscribe),
                  "Invalid object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, FetchEndBeforeStart) {
  MoqtFetch fetch = {
      /*subscribe_id =*/1,
      /*subscriber_priority=*/2,
      /*group_order=*/MoqtDeliveryOrder::kAscending,
      /*joining_fetch=*/std::nullopt,
      /*full_track_name=*/FullTrackName{"foo", "bar"},
      /*start_object=*/FullSequence{1, 2},
      /*end_group=*/1,
      /*end_object=*/1,
      /*parameters=*/
      MoqtSubscribeParameters{"baz", std::nullopt, std::nullopt, std::nullopt},
  };
  quiche::QuicheBuffer buffer;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeFetch(fetch),
                  "Invalid FETCH object range");
  EXPECT_EQ(buffer.size(), 0);
  fetch.end_group = 0;
  fetch.end_object = std::nullopt;
  EXPECT_QUIC_BUG(buffer = framer_.SerializeFetch(fetch),
                  "Invalid FETCH object range");
  EXPECT_EQ(buffer.size(), 0);
}

TEST_F(MoqtFramerSimpleTest, SubscribeUpdateEndGroupOnly) {
  MoqtSubscribeUpdate subscribe_update = {
      /*subscribe_id=*/3,
      /*start=*/FullSequence(4, 3),
      /*end_group=*/4,
      /*subscriber_priority=*/0xaa,
      MoqtSubscribeParameters{std::nullopt, std::nullopt, std::nullopt,
                              std::nullopt},
  };
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeSubscribeUpdate(subscribe_update);
  EXPECT_GT(buffer.size(), 0);
  const uint8_t* end_group = BufferAtOffset(buffer, 5);
  EXPECT_EQ(*end_group, 5);
}

TEST_F(MoqtFramerSimpleTest, SubscribeUpdateIncrementsEnd) {
  MoqtSubscribeUpdate subscribe_update = {
      /*subscribe_id=*/3,
      /*start=*/FullSequence(4, 3),
      /*end_group=*/4,
      /*subscriber_priority=*/0xaa,
      MoqtSubscribeParameters{std::nullopt, std::nullopt, std::nullopt,
                              std::nullopt},
  };
  quiche::QuicheBuffer buffer;
  buffer = framer_.SerializeSubscribeUpdate(subscribe_update);
  EXPECT_GT(buffer.size(), 0);
  const uint8_t* end_group = BufferAtOffset(buffer, 5);
  EXPECT_EQ(*end_group, 5);
}

TEST_F(MoqtFramerSimpleTest, JoiningFetch) {
  JoiningFetchMessage message;
  quiche::QuicheBuffer buffer =
      framer_.SerializeFetch(std::get<MoqtFetch>(message.structured_data()));
  EXPECT_EQ(buffer.size(), message.total_message_size());
  EXPECT_EQ(buffer.AsStringView(), message.PacketSample());
}

}  // namespace moqt::test
