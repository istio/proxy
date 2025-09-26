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
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/test_tools/moqt_parser_test_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_test_message.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/test_tools/in_memory_stream.h"

namespace moqt::test {

namespace {

using ::testing::AnyOf;

constexpr std::array kMessageTypes{
    MoqtMessageType::kSubscribe,
    MoqtMessageType::kSubscribeOk,
    MoqtMessageType::kSubscribeError,
    MoqtMessageType::kSubscribeUpdate,
    MoqtMessageType::kUnsubscribe,
    MoqtMessageType::kPublishDone,
    MoqtMessageType::kTrackStatus,
    MoqtMessageType::kTrackStatusOk,
    MoqtMessageType::kTrackStatusError,
    MoqtMessageType::kPublishNamespace,
    MoqtMessageType::kPublishNamespaceOk,
    MoqtMessageType::kPublishNamespaceError,
    MoqtMessageType::kPublishNamespaceDone,
    MoqtMessageType::kPublishNamespaceCancel,
    MoqtMessageType::kClientSetup,
    MoqtMessageType::kServerSetup,
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
        data_parser_(&data_stream_, &visitor_) {}

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
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->IncreasePayloadLengthByOne();
  ProcessData(message->PacketSample(), false);
  // The parser will actually report a message, because it's all there.
  EXPECT_EQ(visitor_.messages_received_, 1);
  EXPECT_EQ(visitor_.parsing_error_,
            "Message length does not match payload length");
}

TEST_P(MoqtParserTest, PayloadLengthTooShort) {
  if (IsDataStream()) {
    return;
  }
  std::unique_ptr<TestMessageBase> message = MakeMessage();
  message->DecreasePayloadLengthByOne();
  ProcessData(message->PacketSample(), false);
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Message length does not match payload length");
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, true);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(2, 1, false);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(2, 1, false);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false);
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
      0x20, 0x00, 0x0d, 0x02, 0x01, 0x02,  // versions
      0x03,                                // 3 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
      0x02, 0x32,                          // max_request_id = 50
      0x02, 0x32,                          // max_request_id = 50
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, ClientSetupAuthorizationTokenTagRegister) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x13, 0x02, 0x01, 0x02,              // versions
      0x03,                                            // 3 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,                    // path = "foo"
      0x02, 0x32,                                      // max_request_id = 50
      0x03, 0x06, 0x01, 0x10, 0x00, 0x62, 0x61, 0x72,  // REGISTER 0x01
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
      0x21, 0x00, 0x07,
      0x01,                          // version = 1
      0x01,                          // 1 param
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Server SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, SetupAuthorityFromServer) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x21, 0x00, 0x07,
      0x01,                          // version = 1
      0x01,                          // 1 param
      0x05, 0x03, 0x66, 0x6f, 0x6f,  // authority = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Server SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidAuthority);
}

TEST_F(MoqtMessageSpecificTest, SetupPathAppearsTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0e, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x02,                                // 2 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, SetupPathOverWebtrans) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x09, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x01,                                // 1 param
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, SetupAuthorityOverWebtrans) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x09, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x01,                                // 1 param
      0x05, 0x03, 0x66, 0x6f, 0x6f,        // authority = "foo"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidAuthority);
}

TEST_F(MoqtMessageSpecificTest, SetupPathMissing) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x04, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x00,                                // no param
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kInvalidPath);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupMaxRequestIdAppearsTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0d, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x03,                                // 4 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
      0x02, 0x32,                          // max_request_id = 50
      0x02, 0x32,                          // max_request_id = 50
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Client SETUP contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupMalformedPath) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x09, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x01,                                // 1 param
      0x01, 0x03, 0x66, 0x5c, 0x6f,        // path = "f\o"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Malformed path");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kMalformedPath);
}

TEST_F(MoqtMessageSpecificTest, ServerSetupMalformedAuthority) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char setup[] = {
      0x20, 0x00, 0x0e, 0x02, 0x01, 0x02,  // versions = 1, 2
      0x02,                                // 2 params
      0x01, 0x03, 0x66, 0x6f, 0x6f,        // path = "foo"
      0x05, 0x03, 0x66, 0x5c, 0x6f,        // authority = "f\o"
  };
  stream.Receive(absl::string_view(setup, sizeof(setup)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Malformed authority");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kMalformedAuthority);
}

TEST_F(MoqtMessageSpecificTest, UnknownParameterTwiceIsOk) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x1a, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x02,                          // two params
      0x1f, 0x03, 0x62, 0x61, 0x72,  // 0x1f = "bar"
      0x1f, 0x03, 0x62, 0x61, 0x72,  // 0x1f = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, SubscribeDeliveryTimeoutTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x16, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x02,                          // two params
      0x02, 0x67, 0x10,              // delivery_timeout = 10000
      0x02, 0x67, 0x10,              // delivery_timeout = 10000
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "SUBSCRIBE contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeMaxCacheDurationTwice) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x16, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x02,                          // two params
      0x04, 0x67, 0x10,              // max_cache_duration = 10000
      0x04, 0x67, 0x10,              // max_cache_duration = 10000
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "SUBSCRIBE contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeAuthorizationTokenTagDelete) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x14, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x01,                          // one param
      0x03, 0x02, 0x00, 0x00,        // authorization_token = DELETE 0;
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Unknown Auth Token Alias");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, SubscribeAuthorizationTokenTagRegister) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x18, 0x01, 0x01, 0x03, 0x66, 0x6f,
      0x6f,                          // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x01,                          // one param
      0x03, 0x06, 0x01, 0x10, 0x00, 0x62, 0x61, 0x72,  // REGISTER 0x01
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Too many authorization token tags");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kAuthTokenCacheOverflow);
}

TEST_F(MoqtMessageSpecificTest, SubscribeAuthorizationTokenTagUseAlias) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x14, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x01,                          // one param
      0x03, 0x02, 0x02, 0x07,        // authorization_token = USE 7;
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Unknown Auth Token Alias");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest,
       SubscribeAuthorizationTokenTagUnknownAliasType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x14, 0x01, 0x01,
      0x03, 0x66, 0x6f, 0x6f,        // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x02,                          // filter_type = kLatestObject
      0x01,                          // one param
      0x03, 0x02, 0x04, 0x07,        // authorization_token type 4
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid Authorization Token Alias type");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest,
       SubscribeAuthorizationTokenTagUnknownTokenType) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x16, 0x01, 0x01, 0x03,
      0x66, 0x6f, 0x6f,                   // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,       // track_name = "abcd"
      0x20, 0x02, 0x01,                   // priority, order, forward
      0x02,                               // filter_type = kLatestObject
      0x01,                               // one param
      0x03, 0x04, 0x03, 0x01, 0x00, 0x00  // authorization_token type 1
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid Authorization Token Type");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kKeyValueFormattingError);
}

TEST_F(MoqtMessageSpecificTest, SubscribeInvalidGroupOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03,
      0x00,
      0x1c,
      0x01,  // id
      0x01,
      0x03,
      0x66,
      0x6f,
      0x6f,  // track_namespace = "foo"
      0x04,
      0x61,
      0x62,
      0x63,
      0x64,  // track_name = "abcd"
      0x20,  // subscriber priority = 0x20
      0x03,  // group order = invalid
      0x01,  // forward = true
      0x03,  // Filter type: Absolute Start
      0x04,  // start_group = 4 (relative previous)
      0x01,  // start_object = 1 (absolute)
      // No EndGroup or EndObject
      0x02,  // 2 parameters
      0x02,
      0x67,
      0x10,  // delivery_timeout = 10000 ms
      0x03,
      0x05,
      0x03,
      0x00,
      0x62,
      0x61,
      0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid group order value in SUBSCRIBE");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeInvalidForward) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03,
      0x00,
      0x1c,
      0x01,  // id
      0x01,
      0x03,
      0x66,
      0x6f,
      0x6f,  // track_namespace = "foo"
      0x04,
      0x61,
      0x62,
      0x63,
      0x64,  // track_name = "abcd"
      0x20,  // subscriber priority = 0x20
      0x02,  // group order = descending
      0x02,  // forward = invalid
      0x03,  // Filter type: Absolute Start
      0x04,  // start_group = 4 (relative previous)
      0x01,  // start_object = 1 (absolute)
      // No EndGroup or EndObject
      0x02,  // 2 parameters
      0x02,
      0x67,
      0x10,  // delivery_timeout = 10000 ms
      0x03,
      0x05,
      0x03,
      0x00,
      0x62,
      0x61,
      0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid forward value in SUBSCRIBE");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeInvalidFilter) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03,
      0x00,
      0x1c,
      0x01,  // id
      0x01,
      0x03,
      0x66,
      0x6f,
      0x6f,  // track_namespace = "foo"
      0x04,
      0x61,
      0x62,
      0x63,
      0x64,  // track_name = "abcd"
      0x20,  // subscriber priority = 0x20
      0x02,  // group order = descending
      0x01,  // forward = true
      0x05,  // Filter type: Absolute Start
      0x04,  // start_group = 4 (relative previous)
      0x01,  // start_object = 1 (absolute)
      // No EndGroup or EndObject
      0x02,  // 2 parameters
      0x02,
      0x67,
      0x10,  // delivery_timeout = 10000 ms
      0x03,
      0x05,
      0x03,
      0x00,
      0x62,
      0x61,
      0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid filter type");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, SubscribeOkHasAuthorizationToken) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char subscribe_ok[] = {
      0x04, 0x00, 0x12, 0x01, 0x02, 0x03,  // subscribe_id, alias, expires = 3
      0x02, 0x01,                          // group_order = 2, content exists
      0x0c, 0x14,        // largest_group_id = 12, largest_object_id = 20,
      0x02,              // 2 parameters
      0x02, 0x67, 0x10,  // delivery_timeout = 10000
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_token = "bar"
  };
  stream.Receive(absl::string_view(subscribe_ok, sizeof(subscribe_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "SUBSCRIBE_OK contains invalid parameters");
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
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization = "bar"
  };
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, PublishNamespaceHasDeliveryTimeout) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kWebTrans, &stream, visitor_);
  char publish_namespace[] = {
      0x06, 0x00, 0x11, 0x02, 0x01, 0x03, 0x66,
      0x6f, 0x6f,                                // track_namespace = "foo"
      0x02,                                      // 2 params
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_info = "bar"
      0x02, 0x67, 0x10,                          // delivery_timeout = 10000
  };
  stream.Receive(
      absl::string_view(publish_namespace, sizeof(publish_namespace)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "PUBLISH_NAMESPACE contains invalid parameters");
  EXPECT_EQ(visitor_.parsing_error_code_, MoqtError::kProtocolViolation);
}

TEST_F(MoqtMessageSpecificTest, FinMidPayload) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtDataParser parser(&stream, &visitor_);
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, true);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, false);
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
  EXPECT_EQ(visitor_.parsing_error_, "Unknown message type");
}

TEST_F(MoqtMessageSpecificTest, LatestObject) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x17, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority = 0x20, group order, forward
      0x02,                          // filter_type = kLatestObject
      0x01,                          // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  EXPECT_FALSE(message.start.has_value());
  EXPECT_FALSE(message.end_group.has_value());
}

TEST_F(MoqtMessageSpecificTest, InvalidDeliveryOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x17, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x08, 0x01,              // priority, invalid order, forward
      0x01,                          // filter_type = kNextGroupStart
      0x01,                          // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid group order value in SUBSCRIBE");
}

TEST_F(MoqtMessageSpecificTest, AbsoluteStart) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x19, 0x01,                    // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,              // track_name = "abcd"
      0x20, 0x02, 0x01,                          // priority, order, forward
      0x03,                                      // filter_type = kAbsoluteStart
      0x04,                                      // start_group = 4
      0x01,                                      // start_object = 1
      0x01,                                      // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_tag = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  EXPECT_TRUE(message.start.has_value() && message.start->group == 4);
  EXPECT_TRUE(message.start.has_value() && message.start->object == 1);
  EXPECT_FALSE(message.end_group.has_value());
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRange) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x1a, 0x01,                    // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,              // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,              // track_name = "abcd"
      0x20, 0x02, 0x01,                          // priority, order, forward
      0x04,                                      // filter_type = kAbsoluteRange
      0x04,                                      // start_group = 4
      0x01,                                      // start_object = 1
      0x07,                                      // end_group = 7
      0x01,                                      // 1 parameter
      0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x72,  // authorization_info = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  ASSERT_EQ(visitor_.messages_received_, 1);
  EXPECT_FALSE(visitor_.parsing_error_.has_value());
  MoqtSubscribe message =
      std::get<MoqtSubscribe>(visitor_.last_message_.value());
  EXPECT_TRUE(message.start.has_value() && message.start->group == 4);
  EXPECT_TRUE(message.start.has_value() && message.start->object == 1);
  EXPECT_EQ(message.end_group.value(), 7);
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRangeEndGroupTooLow) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x18, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x04,                          // filter_type = kAbsoluteRange
      0x04,                          // start_group = 4
      0x01,                          // start_object = 1
      0x03,                          // end_group = 3
      0x01,                          // 1 parameter
      0x03, 0x03, 0x62, 0x61, 0x72,  // authorization_info = "bar"
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "End group is less than start group");
}

TEST_F(MoqtMessageSpecificTest, AbsoluteRangeExactlyOneObject) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe[] = {
      0x03, 0x00, 0x13, 0x01,        // id
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x04, 0x61, 0x62, 0x63, 0x64,  // track_name = "abcd"
      0x20, 0x02, 0x01,              // priority, order, forward
      0x04,                          // filter_type = kAbsoluteRange
      0x04,                          // start_group = 4
      0x01,                          // start_object = 1
      0x04,                          // end_group = 4
      0x00,                          // no parameters
  };
  stream.Receive(absl::string_view(subscribe, sizeof(subscribe)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, SubscribeUpdateExactlyOneObject) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe_update[] = {
      0x02, 0x00, 0x07, 0x02, 0x03, 0x01, 0x04,  // start and end sequences
      0x20, 0x01,                                // priority, forward
      0x00,                                      // No parameters
  };
  stream.Receive(absl::string_view(subscribe_update, sizeof(subscribe_update)),
                 false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 1);
}

TEST_F(MoqtMessageSpecificTest, SubscribeUpdateEndGroupTooLow) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char subscribe_update[] = {
      0x02, 0x00, 0x09, 0x02, 0x03, 0x01, 0x03,  // start and end sequences
      0x20, 0x01,                                // priority, forward
      0x01,                                      // 1 parameter
      0x02, 0x20,                                // delivery_timeout = 32 ms
  };
  stream.Receive(absl::string_view(subscribe_update, sizeof(subscribe_update)),
                 false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "End group is less than start group");
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

TEST_F(MoqtMessageSpecificTest, DatagramSuccessful) {
  for (MoqtDatagramType datagram_type : AllMoqtDatagramTypes()) {
    ObjectDatagramMessage message(datagram_type);
    MoqtObject object;
    std::optional<absl::string_view> payload =
        ParseDatagram(message.PacketSample(), object);
    ASSERT_TRUE(payload.has_value());
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
    std::optional<absl::string_view> payload =
        ParseDatagram(message.PacketSample(), object);
    ASSERT_TRUE(payload.has_value());
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, true);
  StreamHeaderSubgroupMessage message(type);
  MoqtObject object;
  std::optional<absl::string_view> payload =
      ParseDatagram(message.PacketSample(), object);
  EXPECT_EQ(payload, std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, TruncatedDatagram) {
  ObjectDatagramMessage message(MoqtDatagramType(false, true, false, false));
  message.set_wire_image_size(4);
  MoqtObject object;
  std::optional<absl::string_view> payload =
      ParseDatagram(message.PacketSample(), object);
  EXPECT_EQ(payload, std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, VeryTruncatedDatagram) {
  char message = 0x40;
  MoqtObject object;
  std::optional<absl::string_view> payload =
      ParseDatagram(absl::string_view(&message, sizeof(message)), object);
  EXPECT_EQ(payload, std::nullopt);
}

TEST_F(MoqtMessageSpecificTest, SubscribeOkInvalidContentExists) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  SubscribeOkMessage subscribe_ok;
  subscribe_ok.SetInvalidContentExists();
  stream.Receive(subscribe_ok.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "SUBSCRIBE_OK ContentExists has invalid value");
}

TEST_F(MoqtMessageSpecificTest, SubscribeOkInvalidDeliveryOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  SubscribeOkMessage subscribe_ok;
  subscribe_ok.SetInvalidDeliveryOrder();
  stream.Receive(subscribe_ok.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Invalid group order value in SUBSCRIBE_OK");
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
  EXPECT_EQ(visitor_.parsing_error_,
            "End object comes before start object in FETCH");
}

TEST_F(MoqtMessageSpecificTest, FetchInvalidRange2) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  FetchMessage fetch;
  fetch.SetEndObject(0, std::nullopt);
  stream.Receive(fetch.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "End object comes before start object in FETCH");
}

TEST_F(MoqtMessageSpecificTest, FetchInvalidGroupOrder) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  FetchMessage fetch;
  fetch.SetGroupOrder(3);
  stream.Receive(fetch.PacketSample(), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "Invalid group order value in FETCH message");
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
  EXPECT_EQ(visitor_.parsing_error_, "Invalid number of namespace elements");
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
  EXPECT_EQ(visitor_.parsing_error_, "Invalid number of namespace elements");
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

TEST_F(MoqtMessageSpecificTest, PublishGroupOrder0) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish[27] = {
      0x1d, 0x00, 0x18,
      0x01,                          // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,        // track_name = "bar"
      0x04,                          // track_alias = 4
      0x00,                          // group_order
      0x01, 0x0a, 0x01,              // content exists, largest_location = 10, 1
      0x01,                          // forward = true
      0x01, 0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // parameters = "baz"
  };
  stream.Receive(absl::string_view(publish, sizeof(publish)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid group order value in PUBLISH");
}

TEST_F(MoqtMessageSpecificTest, PublishContentExists2) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish[27] = {
      0x1d, 0x00, 0x18,
      0x01,                          // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,        // track_name = "bar"
      0x04,                          // track_alias = 4
      0x01,                          // group_order
      0x02, 0x0a, 0x01,              // content exists, largest_location = 10, 1
      0x01,                          // forward = true
      0x01, 0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // parameters = "baz"
  };
  stream.Receive(absl::string_view(publish, sizeof(publish)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "PUBLISH ContentExists has invalid value");
}

TEST_F(MoqtMessageSpecificTest, PublishForward2) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish[27] = {
      0x1d, 0x00, 0x18,
      0x01,                          // request_id = 1
      0x01, 0x03, 0x66, 0x6f, 0x6f,  // track_namespace = "foo"
      0x03, 0x62, 0x61, 0x72,        // track_name = "bar"
      0x04,                          // track_alias = 4
      0x01,                          // group_order
      0x01, 0x0a, 0x01,              // content exists, largest_location = 10, 1
      0x02,                          // forward = true
      0x01, 0x03, 0x05, 0x03, 0x00, 0x62, 0x61, 0x7a,  // parameters = "baz"
  };
  stream.Receive(absl::string_view(publish, sizeof(publish)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid forward value in PUBLISH");
}

TEST_F(MoqtMessageSpecificTest, PublishOkForward2) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_ok[15] = {
      0x1e, 0x00, 0x0c,
      0x01,                    // request_id = 1
      0x02,                    // forward
      0x02,                    // subscriber_priority = 2
      0x01,                    // group_order = kAscending
      0x04,                    // filter_type = kAbsoluteRange
      0x05, 0x04,              // start = 5, 4
      0x06,                    // end_group = 6
      0x01, 0x02, 0x67, 0x10,  // delivery_timeout = 10000 ms
  };
  stream.Receive(absl::string_view(publish_ok, sizeof(publish_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid forward value in PUBLISH_OK");
}

TEST_F(MoqtMessageSpecificTest, PublishOkGroupOrder0) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_ok[15] = {
      0x1e, 0x00, 0x0c,
      0x01,                    // request_id = 1
      0x01,                    // forward
      0x02,                    // subscriber_priority = 2
      0x00,                    // group_order
      0x04,                    // filter_type = kAbsoluteRange
      0x05, 0x04,              // start = 5, 4
      0x06,                    // end_group = 6
      0x01, 0x02, 0x67, 0x10,  // delivery_timeout = 10000 ms
  };
  stream.Receive(absl::string_view(publish_ok, sizeof(publish_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid group order value in PUBLISH_OK");
}

TEST_F(MoqtMessageSpecificTest, PublishOkFilter5) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_ok[15] = {
      0x1e, 0x00, 0x0c,
      0x01,                    // request_id = 1
      0x01,                    // forward
      0x02,                    // subscriber_priority = 2
      0x01,                    // group_order
      0x05,                    // filter_type
      0x05, 0x04,              // start = 5, 4
      0x06,                    // end_group = 6
      0x01, 0x02, 0x67, 0x10,  // delivery_timeout = 10000 ms
  };
  stream.Receive(absl::string_view(publish_ok, sizeof(publish_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "Invalid filter type");
}

TEST_F(MoqtMessageSpecificTest, PublishOkEndBeforeStart) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_ok[15] = {
      0x1e, 0x00, 0x0c,
      0x01,                    // request_id = 1
      0x01,                    // forward
      0x02,                    // subscriber_priority = 2
      0x01,                    // group_order
      0x04,                    // filter_type
      0x05, 0x04,              // start = 5, 4
      0x04,                    // end_group = 4
      0x01, 0x02, 0x67, 0x10,  // delivery_timeout = 10000 ms
  };
  stream.Receive(absl::string_view(publish_ok, sizeof(publish_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_, "End group is less than start group");
}

TEST_F(MoqtMessageSpecificTest, PublishOkHasMaxCacheDuration) {
  webtransport::test::InMemoryStream stream(/*stream_id=*/0);
  MoqtControlParser parser(kRawQuic, &stream, visitor_);
  char publish_ok[15] = {
      0x1e, 0x00, 0x0c,
      0x01,                    // request_id = 1
      0x01,                    // forward
      0x02,                    // subscriber_priority = 2
      0x01,                    // group_order
      0x04,                    // filter_type
      0x05, 0x04,              // start = 5, 4
      0x06,                    // end_group = 6
      0x01, 0x04, 0x67, 0x10,  // MaxCacheDuration = 10000
  };
  stream.Receive(absl::string_view(publish_ok, sizeof(publish_ok)), false);
  parser.ReadAndDispatchMessages();
  EXPECT_EQ(visitor_.messages_received_, 0);
  EXPECT_EQ(visitor_.parsing_error_,
            "PUBLISH_OK message contains invalid parameters");
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, false);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(0, 1, true);
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
  MoqtDataStreamType type = MoqtDataStreamType::Subgroup(1, 1, false);
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

}  // namespace moqt::test
