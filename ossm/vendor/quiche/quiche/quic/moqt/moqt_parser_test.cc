// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/moqt_parser_test_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_test_message.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/test_tools/in_memory_stream.h"

namespace moqt::test {

namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::Optional;

constexpr std::array kMessageTypes{
    MoqtMessageType::kRequestOk,
    MoqtMessageType::kRequestError,
    MoqtMessageType::kSubscribe,
    MoqtMessageType::kSubscribeOk,
    MoqtMessageType::kRequestUpdate,
    MoqtMessageType::kUnsubscribe,
    MoqtMessageType::kPublishDone,
    MoqtMessageType::kTrackStatus,
    MoqtMessageType::kPublishNamespace,
    MoqtMessageType::kPublishNamespaceDone,
    MoqtMessageType::kNamespace,
    MoqtMessageType::kNamespaceDone,
    MoqtMessageType::kPublishNamespaceCancel,
    MoqtMessageType::kClientSetup,
    MoqtMessageType::kServerSetup,
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
};

using GeneralizedMessageType =
    std::variant<MoqtMessageType, MoqtDataStreamType>;
}  // namespace

struct MoqtParserTestParams {
  MoqtParserTestParams(MoqtMessageType message_type, bool uses_web_transport)
      : message_type(message_type), uses_web_transport(uses_web_transport) {}
  explicit MoqtParserTestParams(MoqtDataStreamType message_type)
      : message_type(message_type), uses_web_transport(true) {}
  GeneralizedMessageType message_type;
  bool uses_web_transport;
};

std::vector<MoqtParserTestParams> GetMoqtParserTestParams() {
  std::vector<MoqtParserTestParams> params;

  for (MoqtMessageType message_type : kMessageTypes) {
    if (message_type == MoqtMessageType::kClientSetup) {
      for (const bool uses_web_transport : {false, true}) {
        params.push_back(
            MoqtParserTestParams(message_type, uses_web_transport));
      }
    } else {
      // All other types are processed the same for either perspective or
      // transport.
      params.push_back(MoqtParserTestParams(message_type, true));
    }
  }
  for (MoqtDataStreamType type : AllMoqtDataStreamTypes()) {
    params.push_back(MoqtParserTestParams(type));
  }
  return params;
}

std::string TypeFormatter(MoqtMessageType type) {
  return MoqtMessageTypeToString(type);
}
std::string TypeFormatter(MoqtDataStreamType type) {
  return MoqtDataStreamTypeToString(type);
}
std::string ParamNameFormatter(
    const testing::TestParamInfo<MoqtParserTestParams>& info) {
  return std::visit([](auto x) { return TypeFormatter(x); },
                    info.param.message_type) +
         "_" + (info.param.uses_web_transport ? "WebTransport" : "QUIC");
}

class MoqtParserTest
    : public quic::test::QuicTestWithParam<MoqtParserTestParams> {
 public:
  MoqtParserTest()
      : message_type_(GetParam().message_type),
        webtrans_(GetParam().uses_web_transport),
        control_stream_(/*stream_id=*/0),
        control_parser_(GetParam().uses_web_transport, &control_stream_,
                        visitor_),
        data_stream_(/*stream_id=*/0),
        data_parser_(&data_stream_, &visitor_) {
    // The default object has priority 0x07, so setting this will let the
    // parser set the correct value when absent.
    data_parser_.set_default_publisher_priority(0x07);
  }

  bool IsDataStream() {
    return std::holds_alternative<MoqtDataStreamType>(message_type_);
  }

  std::unique_ptr<TestMessageBase> MakeMessage() {
    if (IsDataStream()) {
      return CreateTestDataStream(std::get<MoqtDataStreamType>(message_type_));
    }
    return CreateTestMessage(std::get<MoqtMessageType>(message_type_),
                             webtrans_);
  }

  void ProcessData(absl::string_view data, bool fin) {
    if (IsDataStream()) {
      data_stream_.Receive(data, fin);
      data_parser_.ReadAllData();
      return;
    }
    control_stream_.Receive(data, /*fin=*/false);
    control_parser_.ReadAndDispatchMessages();
  }

 protected:
  MoqtParserTestVisitor visitor_;
  GeneralizedMessageType message_type_;
  bool webtrans_;
  webtransport::test::InMemoryStream control_stream_;
  MoqtControlParser control_parser_;
  webtransport::test::InMemoryStream data_stream_;
  MoqtDataParser data_parser_;
};

INSTANTIATE_TEST_SUITE_P(MoqtParserTests, MoqtParserTest,
                         testing::ValuesIn(GetMoqtParserTestParams()),
                         ParamNameFormatter);

TEST_P(MoqtParserTest, OneMessage) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->MakeObjectEndOfStream();
  ProcessData(message->PacketSample(), true);
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, OneMessageWithLongVarints) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->ExpandVarints();
  ProcessData(message->PacketSample(), false);
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, TwoPartMessage) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->MakeObjectEndOfStream();
  // The test Object message has payload for less then half the message length,
  // so splitting the message in half will prevent the first half from being
  // processed.
  size_t first_data_size = message->total_message_size() / 2;
  ProcessData(message->PacketSample().substr(0, first_data_size), false);
  EXPECT_EQ(visitor_.messages_received_, 0);
  ProcessData(
      message->PacketSample().substr(
          first_data_size, message->total_message_size() - first_data_size),
      true);
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, OneByteAtATime) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->MakeObjectEndOfStream();
  for (size_t i = 0; i < message->total_message_size(); ++i) {
    EXPECT_EQ(visitor_.messages_received_, 0);
    EXPECT_FALSE(visitor_.end_of_message_);
    bool last = i == (message->total_message_size() - 1);
    ProcessData(message->PacketSample().substr(i, 1), last);
  }
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

// In OneByteAtATime, the message is received one byte at a time, and
// immediately processed; here, it is received all at once, but the stream
// receive buffer is represented as a sequence of one-byte chunks.
TEST_P(MoqtParserTest, OneByteAtATimePeek) {
  control_stream_.set_peek_one_byte_at_a_time(true);
  data_stream_.set_peek_one_byte_at_a_time(true);
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->MakeObjectEndOfStream();
  ProcessData(message->PacketSample(), true);
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, OneByteAtATimeLongerVarints) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->ExpandVarints();
  message->MakeObjectEndOfStream();
  for (size_t i = 0; i < message->total_message_size(); ++i) {
    EXPECT_EQ(visitor_.messages_received_, 0);
    EXPECT_FALSE(visitor_.end_of_message_);
    bool last = i == (message->total_message_size() - 1);
    ProcessData(message->PacketSample().substr(i, 1), last);
  }
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, TwoBytesAtATime) {
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->MakeObjectEndOfStream();
  for (size_t i = 0; i < message->total_message_size(); i += 3) {
    EXPECT_EQ(visitor_.messages_received_, 0);
    EXPECT_FALSE(visitor_.end_of_message_);
    bool last = (i + 3) >= message->total_message_size();
    ProcessData(message->PacketSample().substr(i, 3), last);
  }
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  if (IsDataStream()) {
    EXPECT_EQ(visitor_.object_payload(), "foo");
  }
}

TEST_P(MoqtParserTest, EarlyFin) {
  if (!IsDataStream()) {
    return;
  }
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  size_t first_data_size = message->total_message_size() - 1;
  ProcessData(message->PacketSample().substr(0, first_data_size), true);
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_P(MoqtParserTest, SeparateEarlyFin) {
  if (!IsDataStream()) {
    return;
  }
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  size_t first_data_size = message->total_message_size() - 1;
  ProcessData(message->PacketSample().substr(0, first_data_size), false);
  ProcessData(absl::string_view(), true);
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_P(MoqtParserTest, PayloadLengthTooLong) {
  if (IsDataStream()) {
    return;
  }
  MoqtMessageType type = std::get<MoqtMessageType>(message_type_);
  if (type == MoqtMessageType::kSubscribeOk ||
      type == MoqtMessageType::kFetchOk || type == MoqtMessageType::kPublish) {
    // These message types have extensions, which use the length field to
    // determine the size. It is therefore not processed correctly.
    return;
  }
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->IncreasePayloadLengthByOne();
  ProcessData(message->PacketSample(), false);
  // The parser will actually report a message, because it's all there.
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(visitor_.parsing_error_.has_value());
}

TEST_P(MoqtParserTest, PayloadLengthTooShort) {
  if (IsDataStream()) {
    return;
  }
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->DecreasePayloadLengthByOne();
  ProcessData(message->PacketSample(), false);
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_TRUE(visitor_.parsing_error_.has_value());
}

// Tests for message-specific error cases, and behaviors for a single message
// type.
class MoqtMessageSpecificTest : public quic::test::QuicTest {
 public:
  MoqtMessageSpecificTest() {}

  MoqtParserTestVisitor visitor_;

  static constexpr bool kWebTrans = true;
  static constexpr bool kRawQuic = false;
};

// Send the header + some payload, pure payload, then pure payload to end the
// message.
TEST_F(MoqtMessageSpecificTest, ThreePartObject) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, true, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);
  EXPECT_TRUE(message->SetPayloadLength(14));
  message->set_wire_image_size(message->total_message_size() - 11);
  stream.Receive(message->PacketSample(), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_FALSE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "foo");

  // second part
  stream.Receive("bar", false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_FALSE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "foobar");

  // third part includes FIN
  stream.Receive("deadbeef", true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_TRUE(visitor_.fin_received_);
  EXPECT_EQ(visitor_.object_payload(), "foobardeadbeef");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
}

// Send the part of header, rest of header + payload, plus payload.
TEST_F(MoqtMessageSpecificTest, ThreePartObjectFirstIncomplete) {
  uint8_t payload_length = 51;
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(2, 1, false, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);
  EXPECT_TRUE(message->SetPayloadLength(payload_length));

  // first part
  stream.Receive(message->PacketSample().substr(0, 4), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);

  // second part. Add padding to it.
  stream.Receive(
      message->PacketSample().substr(4, message->total_message_size() - 7),
      false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_FALSE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload().length(), payload_length - 3);

  // third part includes FIN
  stream.Receive("bar", true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_TRUE(visitor_.fin_received_);
  EXPECT_EQ(*visitor_.object_payloads_.crbegin(), "bar");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
}

TEST_F(MoqtMessageSpecificTest, ObjectSplitInExtension) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(2, 1, false, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);

  // first part
  stream.Receive(message->PacketSample().substr(0, 10), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);

  // second part
  stream.Receive(
      message->PacketSample().substr(10, sizeof(message->total_message_size())),
      false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(visitor_.last_message_.has_value() &&
              message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
}

TEST_F(MoqtMessageSpecificTest, StreamHeaderSubgroupFollowOn) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  // first part
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false, false);
  auto message1 = std::make_unique<StreamHeaderSubgroupMessage>(type);
  stream.Receive(message1->PacketSample(), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message1->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "foo");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  // second part
  visitor_.object_payloads_.clear();
  auto message2 = std::make_unique<StreamMiddlerSubgroupMessage>(type);
  stream.Receive(message2->PacketSample(), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 2);
  EXPECT_TRUE(message2->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "bar");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
}

TEST_F(MoqtMessageSpecificTest, StreamHeaderSubgroupFollowOnExpandedVarInts) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  // first part
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false, false);
  auto message1 = std::make_unique<StreamHeaderSubgroupMessage>(type);
  message1->ExpandVarints();
  stream.Receive(message1->PacketSample(), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(message1->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "foo");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  // second part
  visitor_.object_payloads_.clear();
  auto message2 = std::make_unique<StreamMiddlerSubgroupMessage>(type);
  message2->ExpandVarints();
  stream.Receive(message2->PacketSample(), false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 2);
  EXPECT_TRUE(message2->EqualFieldValues(*visitor_.last_message_));
  EXPECT_TRUE(visitor_.end_of_message_);
  EXPECT_EQ(visitor_.object_payload(), "bar");
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
}

TEST_F(MoqtMessageSpecificTest, ClientSetupMaxRequestIdAppearsTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0a,
      0x03,                          // 3 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
      0x01, 0x32,                    // max_request_id = 50
      0x00, 0x32,                    // max_request_id = 50
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Duplicate Setup Parameter")));
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupAuthorizationTokenTagRegister) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x0b,
      0x02,                                            // 2 params
      0x02, 0x32,                                      // max_request_id = 50
      0x01, 0x06, 0x01, 0x10, 0x00, 0x62, 0x61, 0x72,  // REGISTER 0x01
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  // No error even though the registration exceeds the max cache size of 0.
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, SetupPathFromServer) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x06,
      0x01,                          // 1 param
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, SetupAuthorityFromServer) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x06,
      0x01,                          // 1 param
      0x05, 0x03, 0x66, 0x6f, 0x6f,  // authority = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidAuthority);
}

TEST_F(MoqtMessageSpecificTest, SetupPathAppearsTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0b,
      0x02,                          // 2 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
      0x00, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Duplicate Setup Parameter")));
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SetupPathOverWebtrans) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x06,
      0x01,                          // 1 param
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, SetupAuthorityOverWebtrans) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x06,
      0x01,                          // 1 param
      0x05, 0x03, 0x66, 0x6f, 0x6f,  // authority = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidAuthority);
}

TEST_F(MoqtMessageSpecificTest, SetupPathMissing) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20,
      0x00,
      0x01,
      0x00,  // no param
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupMaxRequestIdAppearsTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x05, 0x02,  // 2 params
      0x02, 0x32,              // max_request_id = 50
      0x00, 0x32,              // max_request_id = 50
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Duplicate Setup Parameter")));
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, ClientSetupMalformedPath) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x06,
      0x01,                          // 1 param
      0x01, 0x03, 0x66, 0x5c, 0x6f,  // path = "f\o"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kMalformedPath);
}

TEST_F(MoqtMessageSpecificTest, ClientSetupMalformedAuthority) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0b,
      0x02,                          // 2 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
      0x04, 0x03, 0x66, 0x5c, 0x6f,  // authority = "f\o"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kMalformedAuthority);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupUnknownParameterIsOk) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x0b,
      0x02,                          // 2 params
      0x1f, 0x03, 0x62, 0x61, 0x72,  // 0x1f = "bar"
      0x00, 0x03, 0x62, 0x61, 0x72,  // 0x1f = "bar"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  MoqtServerSetup message =
      std::get<MoqtServerSetup>(visitor_.last_message_.value());
  EXPECT_EQ(message.parameters, SetupParameters());
}

TEST_F(MoqtMessageSpecificTest, SubscribeDeliveryTimeoutTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x12, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x02,                          // two params
      0x02, 0x67, 0x10,              // delivery_timeout = 10000
      0x00, 0x67, 0x10,              // delivery_timeout = 10000
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Duplicate Message Parameter")));
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeAuthorizationTokenTagDelete) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x10, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // one param
      0x03, 0x02, 0x00, 0x00         // authorization_token = DELETE 0;
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  ASSERT_FALSE(message.parameters.authorization_tokens.empty());
  EXPECT_EQ(message.parameters.authorization_tokens[0].alias_type,
            AuthTokenAliasType::kDelete);
}

TEST_F(MoqtMessageSpecificTest, SubscribeAuthorizationTokenTagRegister) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x14, 0x01, 0x01, 0x03, 0x66, 0x6f,
      0x6f,                          // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // one param
      0x03, 0x06, 0x01, 0x10, 0x00, 0x62, 0x61, 0x72,  // REGISTER 0x01
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  ASSERT_FALSE(message.parameters.authorization_tokens.empty());
  EXPECT_EQ(message.parameters.authorization_tokens[0].alias_type,
            AuthTokenAliasType::kRegister);
}

TEST_F(MoqtMessageSpecificTest,
       SubscribeAuthorizationTokenTagUnknownAliasType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x10, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // one param
      0x03, 0x02, 0x04, 0x07,        // authorization_token type 4
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest,
       SubscribeAuthorizationTokenTagUnknownTokenType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x12, 0x01, 0x01, 0x03,
      0x66, 0x6f, 0x6f,                   // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,       // track_name = "abcd"
      0x01,                               // one param
      0x03, 0x04, 0x03, 0x01, 0x00, 0x00  // authorization_token type 1
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, SubscribeInvalidForward) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0e, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 2 parameters
      0x10, 0x02                     // forward = 2
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeInvalidFilter) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0f, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x01, 0x10               // filter_type = 0x10
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, PublishNamespaceAuthorizationTokenTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char publish_namespace[] = {
      0x06, 0x00, 0x15, 0x02, 0x01, 0x03, 0x66,
      0x6f, 0x6f,                                // track_namespace = "foo"
      0x02,                                      // 2 params
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization = "bar"
      0x00, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization = "bar"
  };
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, FinMidPayload) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, true, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);
  stream.Receive(
      message->PacketSample().substr(0, message->total_message_size() - 1),
      true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_F(MoqtMessageSpecificTest, FinMidExtension) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);
  // Read up to the extension body and then FIN.
  stream.Receive(message->PacketSample().substr(0, 7), true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_F(MoqtMessageSpecificTest, PartialPayloadThenFin) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, false, false);
  auto message = std::make_unique<StreamHeaderSubgroupMessage>(type);
  stream.Receive(
      message->PacketSample().substr(0, message->total_message_size() - 1),
      false);
  parser.ReadAllData();
  stream.Receive(absl::string_view(), true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_F(MoqtMessageSpecificTest, FinMidVarint) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  stream.Receive("\x40", true);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              AnyOf("FIN after incomplete message",
                    "FIN received at an unexpected point in the stream"));
}

TEST_F(MoqtMessageSpecificTest, ControlStreamFin) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  stream.Receive(absl::string_view(), true);  // Find FIN
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.parsing_error_, "FIN on control stream");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, InvalidObjectStatus) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  char stream_header_subgroup[] = {
      0x15,                    // type field
      0x04, 0x05, 0x08,        // varints
      0x07,                    // publisher priority
      0x06, 0x00, 0x00, 0x0f,  // object middler; status = 0x0f
  };
  stream.Receive(
      absl::string_view(stream_header_subgroup, sizeof(stream_header_subgroup)),
      false);
  parser.ReadAllData();
  EXPECT_EQ(visitor_.parsing_error_, "Invalid object status provided");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, Setup2KB) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char big_message[2 * kMaxMessageHeaderSize];
  quic::QuicDataWriter writer(sizeof(big_message), big_message);
  writer.WriteVarInt62(static_cast<uint64_t>(MoqtMessageType::kServerSetup));
  writer.WriteUInt16(8 + kMaxMessageHeaderSize);
  writer.WriteVarInt62(0x1);                    // version
  writer.WriteVarInt62(0x1);                    // num_params
  writer.WriteVarInt62(0xbeef);                 // unknown param
  writer.WriteVarInt62(kMaxMessageHeaderSize);  // very long parameter
  writer.WriteRepeatedByte(0x04, kMaxMessageHeaderSize);
  // Send incomplete message
  stream.Receive(absl::string_view(big_message, writer.length() - 1), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Cannot parse control messages more than 2048 bytes");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInternalError);
}

TEST_F(MoqtMessageSpecificTest, UnknownMessageType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char message[7];
  quic::QuicDataWriter writer(sizeof(message), message);
  writer.WriteVarInt62(0xbeef);  // unknown message type
  writer.WriteUInt16(0x1);       // length
  writer.WriteVarInt62(0x1);     // payload
  stream.Receive(absl::string_view(message, writer.length()), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Unknown control message type 0xbeef");
}

TEST_F(MoqtMessageSpecificTest, SubscribeNoParameters) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0c, 0x01,        // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x00,                          // 0 parameters
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  EXPECT_FALSE(message.parameters.delivery_timeout.has_value());
  EXPECT_FALSE(message.parameters.forward_has_value());
  EXPECT_FALSE(message.parameters.subscription_filter.has_value());
  EXPECT_FALSE(message.parameters.group_order.has_value());
  EXPECT_FALSE(message.parameters.oack_window_size.has_value());
  EXPECT_TRUE(message.parameters.authorization_tokens.empty());
  EXPECT_FALSE(message.parameters.expires.has_value());
  EXPECT_FALSE(message.parameters.subscriber_priority);
  EXPECT_FALSE(message.parameters.largest_object.has_value());
  EXPECT_FALSE(message.parameters.new_group_request.has_value());
}

TEST_F(MoqtMessageSpecificTest, SubscribeUnknownParameter) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0f, 0x01,        // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 0 parameters
      0x40, 0x60, 0x01,              // unknown parameter = 0x60
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, LargestObject) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0f, 0x01,        // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x01, 0x02,              // filter_type = kLargestObject
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  ASSERT_TRUE(message.parameters.subscription_filter.has_value());
  SubscriptionFilter& filter = *message.parameters.subscription_filter;
  EXPECT_TRUE(filter.type() == MoqtFilterType::kLargestObject);
}

TEST_F(MoqtMessageSpecificTest, InvalidDeliveryOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0e, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x22, 0x03,                    // invalid group order = 3
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, NextGroupStart) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x0f, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x01, 0x01,              // filter_type = kNextGroupStart
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  ASSERT_TRUE(message.parameters.subscription_filter.has_value());
  SubscriptionFilter& filter = *message.parameters.subscription_filter;
  EXPECT_TRUE(filter.type() == MoqtFilterType::kNextGroupStart);
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRange) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x12, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x04, 0x04, 0x04, 0x01,
      0x07  // filter_type = kAbsoluteRange
            // (4,1) to 7
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  ASSERT_TRUE(message.parameters.subscription_filter.has_value());
  SubscriptionFilter& filter = *message.parameters.subscription_filter;
  EXPECT_TRUE(filter.type() == MoqtFilterType::kAbsoluteRange &&
              filter.start() == Location(4, 1) && filter.end_group() == 7);
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRangeEndGroupTooLow) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x12, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x04, 0x04, 0x04, 0x01,
      0x03  // filter_type = kAbsoluteRange
            // (4,1) to 3
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRangeExactlyOneGroup) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x12, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x01,                          // 1 parameter
      0x21, 0x04, 0x04, 0x04, 0x01,
      0x04  // filter_type = kAbsoluteRange
            // (4,1) to 4
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, RequestUpdateEndGroupTooLow) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char request_update[] = {
      0x02, 0x00, 0x09, 0x02, 0x00,              // request IDs
      0x01, 0x21, 0x04, 0x04, 0x04, 0x01, 0x03,  // filter
  };
  stream.Receive(absl::string_view(request_update, sizeof(request_update)),
                 false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(
      visitor_.parsing_error_,
      Optional(HasSubstr(
          "AbsoluteRange filter specified with a start after the end")));
}

TEST_F(MoqtMessageSpecificTest, ObjectAckNegativeDelta) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char object_ack[] = {
      0x71, 0x84, 0x00, 0x05,  // type
      0x01, 0x10, 0x20,        // subscribe ID, group, object
      0x40, 0x81,              // -0x40 time delta
  };
  stream.Receive(absl::string_view(object_ack, sizeof(object_ack)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  ASSERT_EQ(visitor_.messages_received_, 1);
  MoqtObjectAck message =
      std::get<MoqtObjectAck>(visitor_.last_message_.value());
  EXPECT_EQ(message.subscribe_id, 0x01);
  EXPECT_EQ(message.group_id, 0x10);
  EXPECT_EQ(message.object_id, 0x20);
  EXPECT_EQ(message.delta_from_deadline,
            quic::QuicTimeDelta::FromMicroseconds(-0x40));
}

TEST_F(MoqtMessageSpecificTest, AllMessagesTogether) {
  char buffer[5000];
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  size_t write = 0;
  size_t read = 0;
  int fully_received = 0;
  std::unique_ptr<TestMessageBase> prev_message = nullptr;
  for (MoqtMessageType type : kMessageTypes) {
    // Each iteration, process from the halfway point of one message to the
    // halfway point of the next.
    std::unique_ptr<TestMessageBase> message =
        CreateTestMessage(type, kRawQuic);
    memcpy(buffer + write, message->PacketSample().data(),
           message->total_message_size());
    size_t new_read = write + message->total_message_size() / 2;
    stream.Receive(absl::string_view(buffer + read, new_read - read), false);
    parser.ReadAndDispatchMessages();
    ASSERT_EQ(visitor_.messages_received_, fully_received);
    if (prev_message != nullptr) {
      EXPECT_TRUE(prev_message->EqualFieldValues(*visitor_.last_message_));
    }
    fully_received++;
    read = new_read;
    write += message->total_message_size();
    prev_message = std::move(message);
  }
  // Deliver the rest
  stream.Receive(absl::string_view(buffer + read, write - read), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, fully_received);
  EXPECT_TRUE(prev_message->EqualFieldValues(*visitor_.last_message_));
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
}

TEST_F(MoqtMessageSpecificTest, ReadOnlyMessageType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtMessageTypeParser parser(&stream);
  char buffer[] = {0x40, 0x03};
  stream.Receive(absl::string_view(buffer, sizeof(buffer)), false);
  EXPECT_TRUE(parser.ReadUntilMessageTypeKnown());
  EXPECT_EQ(parser.message_type(), 0x03);
}

TEST_F(MoqtMessageSpecificTest, ReadOnlyMessageTypeIncomplete) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtMessageTypeParser parser(&stream);
  char buffer[] = {0x40};
  stream.Receive(absl::string_view(buffer, sizeof(buffer)), false);
  EXPECT_TRUE(parser.ReadUntilMessageTypeKnown());
  EXPECT_FALSE(parser.message_type().has_value());
}

TEST_F(MoqtMessageSpecificTest, ReadOnlyMessageTypeEarlyFin) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtMessageTypeParser parser(&stream);
  char buffer[] = {0x03};
  stream.Receive(absl::string_view(buffer, sizeof(buffer)), true);
  EXPECT_FALSE(parser.ReadUntilMessageTypeKnown());
}

TEST_F(MoqtMessageSpecificTest, DatagramSuccessful) {
  for (MoqtDatagramType datagram_type : AllMoqtDatagramTypes()) {
    ObjectDatagramMessage message(datagram_type);
    MoqtObject object;
    bool use_default_priority;
    std::optional<absl::string_view> payload =
        ParseDatagram(message.PacketSample(), object, use_default_priority);
    EXPECT_EQ(use_default_priority, datagram_type.has_default_priority());
    ASSERT_TRUE(payload.has_value());
    if (use_default_priority) {
      object.publisher_priority = message.publisher_priority();
    }
    TestMessageBase::MessageStructuredData object_metadata =
        TestMessageBase::MessageStructuredData(object);
    EXPECT_TRUE(message.EqualFieldValues(object_metadata));
    if (datagram_type.has_status()) {
      EXPECT_EQ(payload, "");
    } else {
      EXPECT_EQ(payload, "foo");
    }
  }
}

TEST_F(MoqtMessageSpecificTest, DatagramSuccessfulExpandVarints) {
  for (MoqtDatagramType datagram_type : AllMoqtDatagramTypes()) {
    ObjectDatagramMessage message(datagram_type);
    message.ExpandVarints();
    MoqtObject object;
    bool check_priority;
    std::optional<absl::string_view> payload =
        ParseDatagram(message.PacketSample(), object, check_priority);
    EXPECT_EQ(check_priority, datagram_type.has_default_priority());
    ASSERT_TRUE(payload.has_value());
    if (check_priority) {
      object.publisher_priority = message.publisher_priority();
    }
    TestMessageBase::MessageStructuredData object_metadata =
        TestMessageBase::MessageStructuredData(object);
    EXPECT_TRUE(message.EqualFieldValues(object_metadata));
    if (datagram_type.has_status()) {
      EXPECT_EQ(payload, "");
    } else {
      EXPECT_EQ(payload, "foo");
    }
  }
}

TEST_F(MoqtMessageSpecificTest, WrongMessageInDatagram) {
  char payload[] = {0x33, 0x10, 0x20};
  MoqtObject object;
  bool check_priority;
  EXPECT_EQ(ParseDatagram(absl::string_view(payload, sizeof(payload)), object,
                          check_priority),
            std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, TruncatedDatagram) {
  ObjectDatagramMessage message(
      MoqtDatagramType(false, true, false, false, false));
  message.set_wire_image_size(4);
  MoqtObject object;
  bool check_priority;
  EXPECT_EQ(ParseDatagram(message.PacketSample(), object, check_priority),
            std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, VeryTruncatedDatagram) {
  char message = 0x40;
  MoqtObject object;
  bool check_priority;
  EXPECT_EQ(ParseDatagram(absl::string_view(&message, sizeof(message)), object,
                          check_priority),
            std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, SubscribeOkInvalidDeliveryOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  SubscribeOkMessage subscribe_ok;
  subscribe_ok.SetInvalidDeliveryOrder();
  stream.Receive(subscribe_ok.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Invalid SUBSCRIBE_OK track extensions")));
}

TEST_F(MoqtMessageSpecificTest, SubscribeOkExpirationIsZero) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe_ok[] = {
      0x04, 0x00, 0x05, 0x02, 0x01,  // request_id = 2, track_alias = 1
      0x01, 0x08, 0x00               // expires = 0
  };
  stream.Receive(absl::string_view(subscribe_ok, sizeof(subscribe_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  ASSERT_EQ(visitor_.messages_received_, 1);
  MoqtSubscribeOk message =
      std::get<MoqtSubscribeOk>(visitor_.last_message_.value());
  EXPECT_EQ(message.parameters.expires, quic::QuicTimeDelta::Infinite());
}

TEST_F(MoqtMessageSpecificTest, FetchWholeGroup) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  FetchMessage fetch;
  fetch.SetEndObject(5, std::nullopt);
  stream.Receive(fetch.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_TRUE(visitor_.last_message_.has_value());
  if (!visitor_.last_message_.has_value()) {
    return;
  }
  MoqtFetch parse_result = std::get<MoqtFetch>(*visitor_.last_message_);
  auto standalone = std::get<StandaloneFetch>(parse_result.fetch);
  EXPECT_EQ(standalone.end_location, Location(5, kMaxObjectId));
}

TEST_F(MoqtMessageSpecificTest, FetchInvalidRange) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  FetchMessage fetch;
  fetch.SetEndObject(1, 1);
  stream.Receive(fetch.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(
      visitor_.parsing_error_,
      Optional(HasSubstr("End object comes before start object in FETCH")));
}

TEST_F(MoqtMessageSpecificTest, FetchInvalidRange2) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  FetchMessage fetch;
  fetch.SetEndObject(0, std::nullopt);
  stream.Receive(fetch.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_THAT(
      visitor_.parsing_error_,
      Optional(HasSubstr("End object comes before start object in FETCH")));
}

TEST_F(MoqtMessageSpecificTest, PaddingStream) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  std::string buffer(32, '\0');
  quic::QuicDataWriter writer(buffer.size(), buffer.data());
  ASSERT_TRUE(writer.WriteVarInt62(MoqtDataStreamType::Padding().value()));
  for (int i = 0; i < 100; ++i) {
    stream.Receive(buffer, false);
    parser.ReadAllData();
    ASSERT_EQ(visitor_.messages_received_, 0);
    ASSERT_EQ(visitor_.parsing_error_, std::nullopt);
  }
}

// All messages with TrackNamespace use ReadTrackNamespace too check this. Use
// PUBLISH_NAMESPACE.
TEST_F(MoqtMessageSpecificTest, NamespaceTooSmall) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_namespace[7] = {
      0x06, 0x00, 0x04, 0x02,  // request_id = 2
      0x01, 0x00,              // one empty namespace element
      0x00,                    // no parameters
  };
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  --publish_namespace[2];  // Remove one element.
  --publish_namespace[4];
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace) - 1),
      false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Invalid number of namespace elements")));
}

TEST_F(MoqtMessageSpecificTest, NamespaceTooLarge) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_namespace[39] = {
      0x06, 0x00, 0x23, 0x02,  // type, length = 35, request_id = 2
      0x20,                    // 32 namespace elements. This is the maximum.
  };
  // 32 empty namespace elements + no parameters.
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace) - 1),
      false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  ++publish_namespace[2];  // Add one element.
  ++publish_namespace[4];
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_THAT(visitor_.parsing_error_,
              Optional(HasSubstr("Invalid number of namespace elements")));
}

TEST_F(MoqtMessageSpecificTest, RelativeJoiningFetch) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  RelativeJoiningFetchMessage message;
  stream.Receive(message.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  EXPECT_TRUE(visitor_.last_message_.has_value() &&
              message.EqualFieldValues(*visitor_.last_message_));
}

TEST_F(MoqtMessageSpecificTest, AbsoluteJoiningFetch) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  AbsoluteJoiningFetchMessage message;
  stream.Receive(message.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  EXPECT_TRUE(visitor_.last_message_.has_value() &&
              message.EqualFieldValues(*visitor_.last_message_));
}

TEST_F(MoqtMessageSpecificTest, InvalidSubscribeNamespaceOption) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe_namespace[] = {
      0x11, 0x00, 0x11, 0x01,                    // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // namespace = "foo"
      0x03,                                      // subscribe_options invalid
      0x02,                                      // 2 parameters
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
      0x0d, 0x01,                                // forward = true
  };
  stream.Receive(
      absl::string_view(subscribe_namespace, sizeof(subscribe_namespace)),
      false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

class MoqtDataParserStateMachineTest : public quic::test::QuicTest {
 protected:
  MoqtDataParserStateMachineTest()
      : stream_(/*stream_id=*/0), parser_(&stream_, &visitor_) {}

  webtransport::test::InMemoryStream stream_;
  MoqtParserTestVisitor visitor_;
  MoqtDataParser parser_;
};

TEST_F(MoqtDataParserStateMachineTest, ReadAll) {
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false, false);
  stream_.Receive(StreamHeaderSubgroupMessage(type).PacketSample());
  stream_.Receive(StreamMiddlerSubgroupMessage(type).PacketSample());
  parser_.ReadAllData();
  ASSERT_EQ(visitor_.messages_received_, 2);
  EXPECT_EQ(visitor_.object_payloads_[0], "foo");
  EXPECT_EQ(visitor_.object_payloads_[1], "bar");
  stream_.Receive("", /*fin=*/true);
  parser_.ReadAllData();
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  EXPECT_TRUE(visitor_.fin_received_);
}

TEST_F(MoqtDataParserStateMachineTest, ReadObjects) {
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, true, false);
  stream_.Receive(StreamHeaderSubgroupMessage(type).PacketSample());
  stream_.Receive(StreamMiddlerSubgroupMessage(type).PacketSample(),
                  /*fin=*/true);
  parser_.ReadAtMostOneObject();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.object_payloads_[0], "foo");
  parser_.ReadAtMostOneObject();
  ASSERT_EQ(visitor_.messages_received_, 2);
  EXPECT_EQ(visitor_.object_payloads_[1], "bar");
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  EXPECT_TRUE(visitor_.fin_received_);
}

TEST_F(MoqtDataParserStateMachineTest, ReadTypeThenObjects) {
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, false, false);
  stream_.Receive(StreamHeaderSubgroupMessage(type).PacketSample());
  stream_.Receive(StreamMiddlerSubgroupMessage(type).PacketSample(),
                  /*fin=*/true);
  parser_.ReadStreamType();
  ASSERT_EQ(visitor_.messages_received_, 0);
  EXPECT_TRUE(parser_.stream_type().has_value() &&
              parser_.stream_type()->IsSubgroup());
  parser_.ReadAtMostOneObject();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.object_payloads_[0], "foo");
  parser_.ReadAtMostOneObject();
  ASSERT_EQ(visitor_.messages_received_, 2);
  EXPECT_EQ(visitor_.object_payloads_[1], "bar");
  EXPECT_EQ(visitor_.parsing_error_, std::nullopt);
  EXPECT_TRUE(visitor_.fin_received_);
}

TEST_F(MoqtDataParserStateMachineTest, ReadTypeThenObjectsFetch) {
  for (MoqtFetchSerialization serialization : AllMoqtFetchSerializations()) {
    SCOPED_TRACE(testing::Message() << "flags: " << serialization.value());
    MoqtParserTestVisitor visitor;
    webtransport::test::InMemoryStream stream(/*stream_id=*/0);
    MoqtDataParser parser(&stream, &visitor);
    StreamHeaderFetchMessage header;
    StreamMiddlerFetchMessage middler(serialization);
    stream.Receive(header.PacketSample());
    stream.Receive(middler.PacketSample(), /*fin=*/true);
    parser.ReadStreamType();
    ASSERT_EQ(visitor.messages_received_, 0);
    parser.ReadAtMostOneObject();
    ASSERT_EQ(visitor.messages_received_, 1);
    EXPECT_TRUE(header.EqualFieldValues(visitor.last_message_.value()));
    EXPECT_EQ(visitor.object_payloads_[0], "foo");
    parser.ReadAtMostOneObject();
    ASSERT_EQ(visitor.messages_received_, 2);
    EXPECT_TRUE(middler.EqualFieldValues(visitor.last_message_.value()));
    EXPECT_EQ(visitor.object_payloads_[1], "bar");
    EXPECT_EQ(visitor.parsing_error_, std::nullopt);
    EXPECT_TRUE(visitor.fin_received_);
  }
}

TEST_F(MoqtDataParserStateMachineTest, StreamHeaderFetchRefersToPrior) {
  char data[] = {0x05, 0x01, 0x00};
  // Iterate through the 5 serializations that refer to the prior object.
  for (char value : {0x0f, 0x17, 0x1b, 0x1d, 0x1e}) {
    data[2] = value;
    MoqtParserTestVisitor visitor;
    webtransport::test::InMemoryStream stream(/*stream_id=*/0);
    MoqtDataParser parser(&stream, &visitor);
    stream.Receive(absl::string_view(data, sizeof(data)));
    parser.ReadStreamType();
    parser.ReadAtMostOneObject();
    EXPECT_EQ(visitor.parsing_error_,
              "Invalid serialization flags for first object");
    EXPECT_EQ(visitor.parsing_error_code_, MoqtError::kProtocolViolation);
  }
}

TEST_F(MoqtDataParserStateMachineTest, DatagramThenPriorSubgroupId) {
  char data[] = {0x05, 0x01, 0x40, 0x5c, 0x05, 0x01,  // datagram (5, 1)
                 0x80, 0x03, 0x61, 0x61, 0x61,        // priority, payload
                 0xff};  // serialization flag to be overwritten
  // Iterate through the 2 serializations that refer to the prior subgroup.
  for (char value : {0x01, 0x02}) {
    data[11] = value;
    MoqtParserTestVisitor visitor;
    webtransport::test::InMemoryStream stream(/*stream_id=*/0);
    MoqtDataParser parser(&stream, &visitor);
    stream.Receive(absl::string_view(data, sizeof(data)));
    parser.ReadStreamType();
    parser.ReadAtMostOneObject();
    parser.ReadAtMostOneObject();
    EXPECT_EQ(visitor.parsing_error_,
              "reference to subgroup ID of prior datagram");
    EXPECT_EQ(visitor.parsing_error_code_, MoqtError::kProtocolViolation);
  }
}

TEST_F(MoqtDataParserStateMachineTest, InvalidNonexistentRange) {
  char data[] = {0x05, 0x01, 0x40, 0x80};
  stream_.Receive(absl::string_view(data, sizeof(data)));
  parser_.ReadStreamType();
  parser_.ReadAtMostOneObject();
  EXPECT_EQ(visitor_.parsing_error_, "Invalid serialization flags");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtDataParserStateMachineTest, InvalidNonexistentRangeUnknownRange) {
  char data[] = {0x05, 0x01, 0x41, 0x8c};
  stream_.Receive(absl::string_view(data, sizeof(data)));
  parser_.ReadStreamType();
  parser_.ReadAtMostOneObject();
  EXPECT_EQ(visitor_.parsing_error_, "Invalid serialization flags");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtDataParserStateMachineTest, IgnoresEndRangeIndicators) {
  // Header, Range Indicator, Middler
  stream_.Receive(StreamHeaderFetchMessage().PacketSample());
  char data[] = {0x40, 0x8c, 0x05, 0x07,   // non-existent range
                 0x41, 0x0c, 0x05, 0x09};  // unknown range
  stream_.Receive(absl::string_view(data, sizeof(data)));
  std::optional<MoqtFetchSerialization> serialization =
      MoqtFetchSerialization::FromValue(0x40);  // Datagram + explicit object ID
  ASSERT_TRUE(serialization.has_value());
  StreamMiddlerFetchMessage middler(*serialization);
  stream_.Receive(middler.PacketSample(), /*fin=*/true);
  parser_.ReadAllData();
  EXPECT_EQ(visitor_.messages_received_, 2);
  // TODO(martinduke): Once Issue #1506 is resolved, check that the values
  // are reported correctly.
}

}  // namespace moqt::test
