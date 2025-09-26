// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/crypto/transport_parameters.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_tag.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_fuzztest.h"

namespace quic {
namespace test {
namespace {

const QuicVersionLabel kFakeVersionLabel = 0x01234567;
const QuicVersionLabel kFakeVersionLabel2 = 0x89ABCDEF;
const uint64_t kFakeIdleTimeoutMilliseconds = 12012;
const uint64_t kFakeInitialMaxData = 101;
const uint64_t kFakeInitialMaxStreamDataBidiLocal = 2001;
const uint64_t kFakeInitialMaxStreamDataBidiRemote = 2002;
const uint64_t kFakeInitialMaxStreamDataUni = 3000;
const uint64_t kFakeInitialMaxStreamsBidi = 21;
const uint64_t kFakeInitialMaxStreamsUni = 22;
const bool kFakeDisableMigration = true;
const bool kFakeReliableStreamReset = true;
const uint64_t kFakeInitialRoundTripTime = 53;
const uint8_t kFakePreferredStatelessResetTokenData[16] = {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F};

const auto kCustomParameter1 =
    static_cast<TransportParameters::TransportParameterId>(0xffcd);
const char* kCustomParameter1Value = "foo";
const auto kCustomParameter2 =
    static_cast<TransportParameters::TransportParameterId>(0xff34);
const char* kCustomParameter2Value = "bar";

const char kFakeGoogleHandshakeMessage[] =
    "01000106030392655f5230270d4964a4f99b15bbad220736d972aea97bf9ac494ead62e6";

QuicConnectionId CreateFakeOriginalDestinationConnectionId() {
  return TestConnectionId(0x1337);
}

QuicConnectionId CreateFakeInitialSourceConnectionId() {
  return TestConnectionId(0x2345);
}

QuicConnectionId CreateFakeRetrySourceConnectionId() {
  return TestConnectionId(0x9876);
}

QuicConnectionId CreateFakePreferredConnectionId() {
  return TestConnectionId(0xBEEF);
}

std::vector<uint8_t> CreateFakePreferredStatelessResetToken() {
  return std::vector<uint8_t>(
      kFakePreferredStatelessResetTokenData,
      kFakePreferredStatelessResetTokenData +
          sizeof(kFakePreferredStatelessResetTokenData));
}

QuicSocketAddress CreateFakeV4SocketAddress() {
  QuicIpAddress ipv4_address;
  if (!ipv4_address.FromString("65.66.67.68")) {  // 0x41, 0x42, 0x43, 0x44
    QUIC_LOG(FATAL) << "Failed to create IPv4 address";
    return QuicSocketAddress();
  }
  return QuicSocketAddress(ipv4_address, 0x4884);
}

QuicSocketAddress CreateFakeV6SocketAddress() {
  QuicIpAddress ipv6_address;
  if (!ipv6_address.FromString("6061:6263:6465:6667:6869:6A6B:6C6D:6E6F")) {
    QUIC_LOG(FATAL) << "Failed to create IPv6 address";
    return QuicSocketAddress();
  }
  return QuicSocketAddress(ipv6_address, 0x6336);
}

std::unique_ptr<TransportParameters::PreferredAddress>
CreateFakePreferredAddress() {
  TransportParameters::PreferredAddress preferred_address;
  preferred_address.ipv4_socket_address = CreateFakeV4SocketAddress();
  preferred_address.ipv6_socket_address = CreateFakeV6SocketAddress();
  preferred_address.connection_id = CreateFakePreferredConnectionId();
  preferred_address.stateless_reset_token =
      CreateFakePreferredStatelessResetToken();
  return std::make_unique<TransportParameters::PreferredAddress>(
      preferred_address);
}

TransportParameters::LegacyVersionInformation
CreateFakeLegacyVersionInformationClient() {
  TransportParameters::LegacyVersionInformation legacy_version_information;
  legacy_version_information.version = kFakeVersionLabel;
  return legacy_version_information;
}

TransportParameters::LegacyVersionInformation
CreateFakeLegacyVersionInformationServer() {
  TransportParameters::LegacyVersionInformation legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  legacy_version_information.supported_versions.push_back(kFakeVersionLabel);
  legacy_version_information.supported_versions.push_back(kFakeVersionLabel2);
  return legacy_version_information;
}

TransportParameters::VersionInformation CreateFakeVersionInformation() {
  TransportParameters::VersionInformation version_information;
  version_information.chosen_version = kFakeVersionLabel;
  version_information.other_versions.push_back(kFakeVersionLabel);
  version_information.other_versions.push_back(kFakeVersionLabel2);
  return version_information;
}

QuicTagVector CreateFakeGoogleConnectionOptions() {
  return {kALPN, MakeQuicTag('E', 'F', 'G', 0x00),
          MakeQuicTag('H', 'I', 'J', 0xff)};
}

void RemoveGreaseParameters(TransportParameters* params) {
  std::vector<TransportParameters::TransportParameterId> grease_params;
  for (const auto& kv : params->custom_parameters) {
    if (kv.first % 31 == 27) {
      grease_params.push_back(kv.first);
    }
  }
  EXPECT_EQ(grease_params.size(), 1u);
  for (TransportParameters::TransportParameterId param_id : grease_params) {
    params->custom_parameters.erase(param_id);
  }
  // Remove all GREASE versions from version_information.other_versions.
  if (params->version_information.has_value()) {
    QuicVersionLabelVector& other_versions =
        params->version_information.value().other_versions;
    for (auto it = other_versions.begin(); it != other_versions.end();) {
      if ((*it & 0x0f0f0f0f) == 0x0a0a0a0a) {
        it = other_versions.erase(it);
      } else {
        ++it;
      }
    }
  }
}

class TransportParametersTest : public QuicTestWithParam<ParsedQuicVersion> {
 protected:
  TransportParametersTest() : version_(GetParam()) {}

  ParsedQuicVersion version_;
};

INSTANTIATE_TEST_SUITE_P(TransportParametersTests, TransportParametersTest,
                         ::testing::ValuesIn(AllSupportedVersionsWithTls()),
                         ::testing::PrintToStringParamName());

TEST_P(TransportParametersTest, Comparator) {
  TransportParameters orig_params;
  TransportParameters new_params;
  // Test comparison on primitive members.
  orig_params.perspective = Perspective::IS_CLIENT;
  new_params.perspective = Perspective::IS_SERVER;
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  new_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.version_information = CreateFakeVersionInformation();
  new_params.version_information = CreateFakeVersionInformation();
  orig_params.disable_active_migration = true;
  new_params.disable_active_migration = true;
  orig_params.reliable_stream_reset = true;
  new_params.reliable_stream_reset = true;
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);

  // Test comparison on vectors.
  orig_params.legacy_version_information.value().supported_versions.push_back(
      kFakeVersionLabel);
  new_params.legacy_version_information.value().supported_versions.push_back(
      kFakeVersionLabel2);
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.legacy_version_information.value().supported_versions.pop_back();
  new_params.legacy_version_information.value().supported_versions.push_back(
      kFakeVersionLabel);
  orig_params.stateless_reset_token = CreateStatelessResetTokenForTest();
  new_params.stateless_reset_token = CreateStatelessResetTokenForTest();
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);

  // Test comparison on IntegerParameters.
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  new_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest + 1);
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);

  // Test comparison on PreferredAddress
  orig_params.preferred_address = CreateFakePreferredAddress();
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.preferred_address = CreateFakePreferredAddress();
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);

  // Test comparison on CustomMap
  orig_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  orig_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;

  new_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;
  new_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);

  // Test comparison on connection IDs.
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  new_params.initial_source_connection_id = std::nullopt;
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.initial_source_connection_id = TestConnectionId(0xbadbad);
  EXPECT_NE(orig_params, new_params);
  EXPECT_FALSE(orig_params == new_params);
  EXPECT_TRUE(orig_params != new_params);
  new_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  EXPECT_EQ(orig_params, new_params);
  EXPECT_TRUE(orig_params == new_params);
  EXPECT_FALSE(orig_params != new_params);
}

TEST_P(TransportParametersTest, CopyConstructor) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.version_information = CreateFakeVersionInformation();
  orig_params.original_destination_connection_id =
      CreateFakeOriginalDestinationConnectionId();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.stateless_reset_token = CreateStatelessResetTokenForTest();
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  orig_params.initial_max_data.set_value(kFakeInitialMaxData);
  orig_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal);
  orig_params.initial_max_stream_data_bidi_remote.set_value(
      kFakeInitialMaxStreamDataBidiRemote);
  orig_params.initial_max_stream_data_uni.set_value(
      kFakeInitialMaxStreamDataUni);
  orig_params.initial_max_streams_bidi.set_value(kFakeInitialMaxStreamsBidi);
  orig_params.initial_max_streams_uni.set_value(kFakeInitialMaxStreamsUni);
  orig_params.ack_delay_exponent.set_value(kAckDelayExponentForTest);
  orig_params.max_ack_delay.set_value(kMaxAckDelayForTest);
  orig_params.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
  orig_params.disable_active_migration = kFakeDisableMigration;
  orig_params.reliable_stream_reset = kFakeReliableStreamReset;
  orig_params.preferred_address = CreateFakePreferredAddress();
  orig_params.active_connection_id_limit.set_value(
      kActiveConnectionIdLimitForTest);
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  orig_params.retry_source_connection_id = CreateFakeRetrySourceConnectionId();
  orig_params.initial_round_trip_time_us.set_value(kFakeInitialRoundTripTime);
  orig_params.discard_length = 2000;
  std::string google_handshake_message;
  ASSERT_TRUE(absl::HexStringToBytes(kFakeGoogleHandshakeMessage,
                                     &google_handshake_message));
  orig_params.google_handshake_message = std::move(google_handshake_message);
  orig_params.google_connection_options = CreateFakeGoogleConnectionOptions();
  orig_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  orig_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;

  TransportParameters new_params(orig_params);
  EXPECT_EQ(new_params, orig_params);
}

TEST_P(TransportParametersTest, RoundTripClient) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.version_information = CreateFakeVersionInformation();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  orig_params.initial_max_data.set_value(kFakeInitialMaxData);
  orig_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal);
  orig_params.initial_max_stream_data_bidi_remote.set_value(
      kFakeInitialMaxStreamDataBidiRemote);
  orig_params.initial_max_stream_data_uni.set_value(
      kFakeInitialMaxStreamDataUni);
  orig_params.initial_max_streams_bidi.set_value(kFakeInitialMaxStreamsBidi);
  orig_params.initial_max_streams_uni.set_value(kFakeInitialMaxStreamsUni);
  orig_params.ack_delay_exponent.set_value(kAckDelayExponentForTest);
  orig_params.max_ack_delay.set_value(kMaxAckDelayForTest);
  orig_params.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
  orig_params.disable_active_migration = kFakeDisableMigration;
  orig_params.reliable_stream_reset = kFakeReliableStreamReset;
  orig_params.active_connection_id_limit.set_value(
      kActiveConnectionIdLimitForTest);
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  orig_params.initial_round_trip_time_us.set_value(kFakeInitialRoundTripTime);
  orig_params.discard_length = 2000;
  std::string google_handshake_message;
  ASSERT_TRUE(absl::HexStringToBytes(kFakeGoogleHandshakeMessage,
                                     &google_handshake_message));
  orig_params.google_handshake_message = std::move(google_handshake_message);
  orig_params.google_connection_options = CreateFakeGoogleConnectionOptions();
  orig_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  orig_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParameters(orig_params, &serialized));

  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                       serialized.data(), serialized.size(),
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());
  RemoveGreaseParameters(&new_params);
  EXPECT_EQ(new_params, orig_params);
}

TEST_P(TransportParametersTest, RoundTripServer) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_SERVER;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationServer();
  orig_params.version_information = CreateFakeVersionInformation();
  orig_params.original_destination_connection_id =
      CreateFakeOriginalDestinationConnectionId();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.stateless_reset_token = CreateStatelessResetTokenForTest();
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  orig_params.initial_max_data.set_value(kFakeInitialMaxData);
  orig_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal);
  orig_params.initial_max_stream_data_bidi_remote.set_value(
      kFakeInitialMaxStreamDataBidiRemote);
  orig_params.initial_max_stream_data_uni.set_value(
      kFakeInitialMaxStreamDataUni);
  orig_params.initial_max_streams_bidi.set_value(kFakeInitialMaxStreamsBidi);
  orig_params.initial_max_streams_uni.set_value(kFakeInitialMaxStreamsUni);
  orig_params.ack_delay_exponent.set_value(kAckDelayExponentForTest);
  orig_params.max_ack_delay.set_value(kMaxAckDelayForTest);
  orig_params.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
  orig_params.disable_active_migration = kFakeDisableMigration;
  orig_params.reliable_stream_reset = kFakeReliableStreamReset;
  orig_params.preferred_address = CreateFakePreferredAddress();
  orig_params.active_connection_id_limit.set_value(
      kActiveConnectionIdLimitForTest);
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  orig_params.retry_source_connection_id = CreateFakeRetrySourceConnectionId();
  orig_params.google_connection_options = CreateFakeGoogleConnectionOptions();

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParameters(orig_params, &serialized));

  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_SERVER,
                                       serialized.data(), serialized.size(),
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());
  RemoveGreaseParameters(&new_params);
  EXPECT_EQ(new_params, orig_params);
}

TEST_P(TransportParametersTest, AreValid) {
  {
    TransportParameters params;
    std::string error_details;
    params.perspective = Perspective::IS_CLIENT;
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
  }
  {
    TransportParameters params;
    std::string error_details;
    params.perspective = Perspective::IS_CLIENT;
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_idle_timeout_ms.set_value(601000);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
  }
  {
    TransportParameters params;
    std::string error_details;
    params.perspective = Perspective::IS_CLIENT;
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_udp_payload_size.set_value(1200);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_udp_payload_size.set_value(65535);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_udp_payload_size.set_value(9999999);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.max_udp_payload_size.set_value(0);
    error_details = "";
    EXPECT_FALSE(params.AreValid(&error_details));
    EXPECT_EQ(error_details,
              "Invalid transport parameters [Client max_udp_payload_size 0 "
              "(Invalid)]");
    params.max_udp_payload_size.set_value(1199);
    error_details = "";
    EXPECT_FALSE(params.AreValid(&error_details));
    EXPECT_EQ(error_details,
              "Invalid transport parameters [Client max_udp_payload_size 1199 "
              "(Invalid)]");
  }
  {
    TransportParameters params;
    std::string error_details;
    params.perspective = Perspective::IS_CLIENT;
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.ack_delay_exponent.set_value(0);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.ack_delay_exponent.set_value(20);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.ack_delay_exponent.set_value(21);
    EXPECT_FALSE(params.AreValid(&error_details));
    EXPECT_EQ(error_details,
              "Invalid transport parameters [Client ack_delay_exponent 21 "
              "(Invalid)]");
  }
  {
    TransportParameters params;
    std::string error_details;
    params.perspective = Perspective::IS_CLIENT;
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.active_connection_id_limit.set_value(2);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.active_connection_id_limit.set_value(999999);
    EXPECT_TRUE(params.AreValid(&error_details));
    EXPECT_TRUE(error_details.empty());
    params.active_connection_id_limit.set_value(1);
    EXPECT_FALSE(params.AreValid(&error_details));
    EXPECT_EQ(error_details,
              "Invalid transport parameters [Client active_connection_id_limit"
              " 1 (Invalid)]");
    params.active_connection_id_limit.set_value(0);
    EXPECT_FALSE(params.AreValid(&error_details));
    EXPECT_EQ(error_details,
              "Invalid transport parameters [Client active_connection_id_limit"
              " 0 (Invalid)]");
  }
}

TEST_P(TransportParametersTest, NoClientParamsWithStatelessResetToken) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.stateless_reset_token = CreateStatelessResetTokenForTest();
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);

  std::vector<uint8_t> out;
  EXPECT_QUIC_BUG(
      EXPECT_FALSE(SerializeTransportParameters(orig_params, &out)),
      "Not serializing invalid transport parameters: Client cannot send "
      "stateless reset token");
}

TEST_P(TransportParametersTest, ParseClientParams) {
  // clang-format off
  const uint8_t kClientParams[] = {
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // max_udp_payload_size
      0x03,  // parameter id
      0x02,  // length
      0x63, 0x29,  // value
      // initial_max_data
      0x04,  // parameter id
      0x02,  // length
      0x40, 0x65,  // value
      // initial_max_stream_data_bidi_local
      0x05,  // parameter id
      0x02,  // length
      0x47, 0xD1,  // value
      // initial_max_stream_data_bidi_remote
      0x06,  // parameter id
      0x02,  // length
      0x47, 0xD2,  // value
      // initial_max_stream_data_uni
      0x07,  // parameter id
      0x02,  // length
      0x4B, 0xB8,  // value
      // initial_max_streams_bidi
      0x08,  // parameter id
      0x01,  // length
      0x15,  // value
      // initial_max_streams_uni
      0x09,  // parameter id
      0x01,  // length
      0x16,  // value
      // ack_delay_exponent
      0x0a,  // parameter id
      0x01,  // length
      0x0a,  // value
      // max_ack_delay
      0x0b,  // parameter id
      0x01,  // length
      0x33,  // value
      // min_ack_delay_us_draft10
      0xc0, 0x00, 0x00, 0x00, 0xff, 0x04, 0xde, 0x1b,  // parameter id
      0x02,  // length
      0x43, 0xe8,  // value
      // disable_active_migration
      0x0c,  // parameter id
      0x00,  // length
      // reliable_stream_reset
      0xc0, 0x17, 0xf7, 0x58, 0x6d, 0x2c, 0xb5, 0x71,  // parameter id
      0x00,  // length
      // active_connection_id_limit
      0x0e,  // parameter id
      0x01,  // length
      0x34,  // value
      // initial_source_connection_id
      0x0f,  // parameter id
      0x08,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
      // discard
      0x57, 0x3e,  // parameter id
      0x10,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // google_handshake_message
      0x66, 0xab,  // parameter id
      0x24,  // length
      0x01, 0x00, 0x01, 0x06, 0x03, 0x03, 0x92, 0x65, 0x5f, 0x52, 0x30, 0x27,
      0x0d, 0x49, 0x64, 0xa4, 0xf9, 0x9b, 0x15, 0xbb, 0xad, 0x22, 0x07, 0x36,
      0xd9, 0x72, 0xae, 0xa9, 0x7b, 0xf9, 0xac, 0x49, 0x4e, 0xad, 0x62, 0xe6,
      // initial_round_trip_time_us
      0x71, 0x27,  // parameter id
      0x01,  // length
      0x35,  // value
      // google_connection_options
      0x71, 0x28,  // parameter id
      0x0c,  // length
      'A', 'L', 'P', 'N',  // value
      'E', 'F', 'G', 0x00,
      'H', 'I', 'J', 0xff,
      // Google version extension
      0x80, 0x00, 0x47, 0x52,  // parameter id
      0x04,  // length
      0x01, 0x23, 0x45, 0x67,  // initial version
      // version_information
      0x11,  // parameter id
      0x0C,  // length
      0x01, 0x23, 0x45, 0x67,  // chosen version
      0x01, 0x23, 0x45, 0x67,  // other version 1
      0x89, 0xab, 0xcd, 0xef,  // other version 2
  };
  // clang-format on
  const uint8_t* client_params =
      reinterpret_cast<const uint8_t*>(kClientParams);
  size_t client_params_length = ABSL_ARRAYSIZE(kClientParams);
  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                       client_params, client_params_length,
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());
  EXPECT_EQ(Perspective::IS_CLIENT, new_params.perspective);
  ASSERT_TRUE(new_params.legacy_version_information.has_value());
  EXPECT_EQ(kFakeVersionLabel,
            new_params.legacy_version_information.value().version);
  EXPECT_TRUE(
      new_params.legacy_version_information.value().supported_versions.empty());
  ASSERT_TRUE(new_params.version_information.has_value());
  EXPECT_EQ(new_params.version_information.value(),
            CreateFakeVersionInformation());
  EXPECT_FALSE(new_params.original_destination_connection_id.has_value());
  EXPECT_EQ(kFakeIdleTimeoutMilliseconds,
            new_params.max_idle_timeout_ms.value());
  EXPECT_TRUE(new_params.stateless_reset_token.empty());
  EXPECT_EQ(kMaxPacketSizeForTest, new_params.max_udp_payload_size.value());
  EXPECT_EQ(kFakeInitialMaxData, new_params.initial_max_data.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataBidiLocal,
            new_params.initial_max_stream_data_bidi_local.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataBidiRemote,
            new_params.initial_max_stream_data_bidi_remote.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataUni,
            new_params.initial_max_stream_data_uni.value());
  EXPECT_EQ(kFakeInitialMaxStreamsBidi,
            new_params.initial_max_streams_bidi.value());
  EXPECT_EQ(kFakeInitialMaxStreamsUni,
            new_params.initial_max_streams_uni.value());
  EXPECT_EQ(kAckDelayExponentForTest, new_params.ack_delay_exponent.value());
  EXPECT_EQ(kMaxAckDelayForTest, new_params.max_ack_delay.value());
  EXPECT_EQ(kMinAckDelayUsForTest, *new_params.min_ack_delay_us_draft10);
  EXPECT_EQ(kFakeDisableMigration, new_params.disable_active_migration);
  EXPECT_EQ(kFakeReliableStreamReset, new_params.reliable_stream_reset);
  EXPECT_EQ(kActiveConnectionIdLimitForTest,
            new_params.active_connection_id_limit.value());
  ASSERT_TRUE(new_params.initial_source_connection_id.has_value());
  EXPECT_EQ(CreateFakeInitialSourceConnectionId(),
            new_params.initial_source_connection_id.value());
  EXPECT_FALSE(new_params.retry_source_connection_id.has_value());
  EXPECT_EQ(kFakeInitialRoundTripTime,
            new_params.initial_round_trip_time_us.value());
  ASSERT_TRUE(new_params.google_connection_options.has_value());
  EXPECT_EQ(CreateFakeGoogleConnectionOptions(),
            new_params.google_connection_options.value());
  EXPECT_EQ(16, new_params.discard_length);
  std::string expected_google_handshake_message;
  ASSERT_TRUE(absl::HexStringToBytes(kFakeGoogleHandshakeMessage,
                                     &expected_google_handshake_message));
  EXPECT_EQ(expected_google_handshake_message,
            new_params.google_handshake_message);
}

TEST_P(TransportParametersTest,
       ParseClientParamsFailsWithFullStatelessResetToken) {
  // clang-format off
  const uint8_t kClientParamsWithFullToken[] = {
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // stateless_reset_token
      0x02,  // parameter id
      0x10,  // length
      0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
      0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
      // max_udp_payload_size
      0x03,  // parameter id
      0x02,  // length
      0x63, 0x29,  // value
      // initial_max_data
      0x04,  // parameter id
      0x02,  // length
      0x40, 0x65,  // value
  };
  // clang-format on
  const uint8_t* client_params =
      reinterpret_cast<const uint8_t*>(kClientParamsWithFullToken);
  size_t client_params_length = ABSL_ARRAYSIZE(kClientParamsWithFullToken);
  TransportParameters out_params;
  std::string error_details;
  EXPECT_FALSE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                        client_params, client_params_length,
                                        &out_params, &error_details));
  EXPECT_EQ(error_details, "Client cannot send stateless reset token");
}

TEST_P(TransportParametersTest,
       ParseClientParamsFailsWithEmptyStatelessResetToken) {
  // clang-format off
  const uint8_t kClientParamsWithEmptyToken[] = {
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // stateless_reset_token
      0x02,  // parameter id
      0x00,  // length
      // max_udp_payload_size
      0x03,  // parameter id
      0x02,  // length
      0x63, 0x29,  // value
      // initial_max_data
      0x04,  // parameter id
      0x02,  // length
      0x40, 0x65,  // value
  };
  // clang-format on
  const uint8_t* client_params =
      reinterpret_cast<const uint8_t*>(kClientParamsWithEmptyToken);
  size_t client_params_length = ABSL_ARRAYSIZE(kClientParamsWithEmptyToken);
  TransportParameters out_params;
  std::string error_details;
  EXPECT_FALSE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                        client_params, client_params_length,
                                        &out_params, &error_details));
  EXPECT_EQ(error_details,
            "Received stateless_reset_token of invalid length 0");
}

TEST_P(TransportParametersTest, ParseClientParametersRepeated) {
  // clang-format off
  const uint8_t kClientParamsRepeated[] = {
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // max_udp_payload_size
      0x03,  // parameter id
      0x02,  // length
      0x63, 0x29,  // value
      // max_idle_timeout (repeated)
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
  };
  // clang-format on
  const uint8_t* client_params =
      reinterpret_cast<const uint8_t*>(kClientParamsRepeated);
  size_t client_params_length = ABSL_ARRAYSIZE(kClientParamsRepeated);
  TransportParameters out_params;
  std::string error_details;
  EXPECT_FALSE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                        client_params, client_params_length,
                                        &out_params, &error_details));
  EXPECT_EQ(error_details, "Received a second max_idle_timeout");
}

TEST_P(TransportParametersTest, ParseServerParams) {
  // clang-format off
  const uint8_t kServerParams[] = {
      // original_destination_connection_id
      0x00,  // parameter id
      0x08,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x37,
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // stateless_reset_token
      0x02,  // parameter id
      0x10,  // length
      0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
      0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
      // max_udp_payload_size
      0x03,  // parameter id
      0x02,  // length
      0x63, 0x29,  // value
      // initial_max_data
      0x04,  // parameter id
      0x02,  // length
      0x40, 0x65,  // value
      // initial_max_stream_data_bidi_local
      0x05,  // parameter id
      0x02,  // length
      0x47, 0xD1,  // value
      // initial_max_stream_data_bidi_remote
      0x06,  // parameter id
      0x02,  // length
      0x47, 0xD2,  // value
      // initial_max_stream_data_uni
      0x07,  // parameter id
      0x02,  // length
      0x4B, 0xB8,  // value
      // initial_max_streams_bidi
      0x08,  // parameter id
      0x01,  // length
      0x15,  // value
      // initial_max_streams_uni
      0x09,  // parameter id
      0x01,  // length
      0x16,  // value
      // ack_delay_exponent
      0x0a,  // parameter id
      0x01,  // length
      0x0a,  // value
      // max_ack_delay
      0x0b,  // parameter id
      0x01,  // length
      0x33,  // value
      // min_ack_delay_us_draft10
      0xc0, 0x00, 0x00, 0x00, 0xff, 0x04, 0xde, 0x1b,  // parameter id
      0x02,  // length
      0x43, 0xe8,  // value
      // disable_active_migration
      0x0c,  // parameter id
      0x00,  // length
      // reliable_stream_reset
      0xc0, 0x17, 0xf7, 0x58, 0x6d, 0x2c, 0xb5, 0x71,  // parameter id
      0x00,  // length
      // preferred_address
      0x0d,  // parameter id
      0x31,  // length
      0x41, 0x42, 0x43, 0x44,  // IPv4 address
      0x48, 0x84,  // IPv4 port
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  // IPv6 address
      0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
      0x63, 0x36,  // IPv6 port
      0x08,        // connection ID length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBE, 0xEF,  // connection ID
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  // stateless reset token
      0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
      // active_connection_id_limit
      0x0e,  // parameter id
      0x01,  // length
      0x34,  // value
      // initial_source_connection_id
      0x0f,  // parameter id
      0x08,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
      // retry_source_connection_id
      0x10,  // parameter id
      0x08,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x76,
      // google_connection_options
      0x71, 0x28,  // parameter id
      0x0c,  // length
      'A', 'L', 'P', 'N',  // value
      'E', 'F', 'G', 0x00,
      'H', 'I', 'J', 0xff,
      // Google version extension
      0x80, 0x00, 0x47, 0x52,  // parameter id
      0x0d,  // length
      0x01, 0x23, 0x45, 0x67,  // negotiated_version
      0x08,  // length of supported versions array
      0x01, 0x23, 0x45, 0x67,
      0x89, 0xab, 0xcd, 0xef,
      // version_information
      0x11,  // parameter id
      0x0C,  // length
      0x01, 0x23, 0x45, 0x67,  // chosen version
      0x01, 0x23, 0x45, 0x67,  // other version 1
      0x89, 0xab, 0xcd, 0xef,  // other version 2
  };
  // clang-format on
  const uint8_t* server_params =
      reinterpret_cast<const uint8_t*>(kServerParams);
  size_t server_params_length = ABSL_ARRAYSIZE(kServerParams);
  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_SERVER,
                                       server_params, server_params_length,
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());
  EXPECT_EQ(Perspective::IS_SERVER, new_params.perspective);
  ASSERT_TRUE(new_params.legacy_version_information.has_value());
  EXPECT_EQ(kFakeVersionLabel,
            new_params.legacy_version_information.value().version);
  ASSERT_EQ(
      2u,
      new_params.legacy_version_information.value().supported_versions.size());
  EXPECT_EQ(
      kFakeVersionLabel,
      new_params.legacy_version_information.value().supported_versions[0]);
  EXPECT_EQ(
      kFakeVersionLabel2,
      new_params.legacy_version_information.value().supported_versions[1]);
  ASSERT_TRUE(new_params.version_information.has_value());
  EXPECT_EQ(new_params.version_information.value(),
            CreateFakeVersionInformation());
  ASSERT_TRUE(new_params.original_destination_connection_id.has_value());
  EXPECT_EQ(CreateFakeOriginalDestinationConnectionId(),
            new_params.original_destination_connection_id.value());
  EXPECT_EQ(kFakeIdleTimeoutMilliseconds,
            new_params.max_idle_timeout_ms.value());
  EXPECT_EQ(CreateStatelessResetTokenForTest(),
            new_params.stateless_reset_token);
  EXPECT_EQ(kMaxPacketSizeForTest, new_params.max_udp_payload_size.value());
  EXPECT_EQ(kFakeInitialMaxData, new_params.initial_max_data.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataBidiLocal,
            new_params.initial_max_stream_data_bidi_local.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataBidiRemote,
            new_params.initial_max_stream_data_bidi_remote.value());
  EXPECT_EQ(kFakeInitialMaxStreamDataUni,
            new_params.initial_max_stream_data_uni.value());
  EXPECT_EQ(kFakeInitialMaxStreamsBidi,
            new_params.initial_max_streams_bidi.value());
  EXPECT_EQ(kFakeInitialMaxStreamsUni,
            new_params.initial_max_streams_uni.value());
  EXPECT_EQ(kAckDelayExponentForTest, new_params.ack_delay_exponent.value());
  EXPECT_EQ(kMaxAckDelayForTest, new_params.max_ack_delay.value());
  EXPECT_EQ(kMinAckDelayUsForTest, *new_params.min_ack_delay_us_draft10);
  EXPECT_EQ(kFakeDisableMigration, new_params.disable_active_migration);
  EXPECT_EQ(kFakeReliableStreamReset, new_params.reliable_stream_reset);
  ASSERT_NE(nullptr, new_params.preferred_address.get());
  EXPECT_EQ(CreateFakeV4SocketAddress(),
            new_params.preferred_address->ipv4_socket_address);
  EXPECT_EQ(CreateFakeV6SocketAddress(),
            new_params.preferred_address->ipv6_socket_address);
  EXPECT_EQ(CreateFakePreferredConnectionId(),
            new_params.preferred_address->connection_id);
  EXPECT_EQ(CreateFakePreferredStatelessResetToken(),
            new_params.preferred_address->stateless_reset_token);
  EXPECT_EQ(kActiveConnectionIdLimitForTest,
            new_params.active_connection_id_limit.value());
  ASSERT_TRUE(new_params.initial_source_connection_id.has_value());
  EXPECT_EQ(CreateFakeInitialSourceConnectionId(),
            new_params.initial_source_connection_id.value());
  ASSERT_TRUE(new_params.retry_source_connection_id.has_value());
  EXPECT_EQ(CreateFakeRetrySourceConnectionId(),
            new_params.retry_source_connection_id.value());
  ASSERT_TRUE(new_params.google_connection_options.has_value());
  EXPECT_EQ(CreateFakeGoogleConnectionOptions(),
            new_params.google_connection_options.value());
}

TEST_P(TransportParametersTest, ParseServerParametersRepeated) {
  // clang-format off
  const uint8_t kServerParamsRepeated[] = {
      // original_destination_connection_id
      0x00,  // parameter id
      0x08,  // length
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x37,
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // stateless_reset_token
      0x02,  // parameter id
      0x10,  // length
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
      // max_idle_timeout (repeated)
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
  };
  // clang-format on
  const uint8_t* server_params =
      reinterpret_cast<const uint8_t*>(kServerParamsRepeated);
  size_t server_params_length = ABSL_ARRAYSIZE(kServerParamsRepeated);
  TransportParameters out_params;
  std::string error_details;
  EXPECT_FALSE(ParseTransportParameters(version_, Perspective::IS_SERVER,
                                        server_params, server_params_length,
                                        &out_params, &error_details));
  EXPECT_EQ(error_details, "Received a second max_idle_timeout");
}

TEST_P(TransportParametersTest,
       ParseServerParametersEmptyOriginalConnectionId) {
  // clang-format off
  const uint8_t kServerParamsEmptyOriginalConnectionId[] = {
      // original_destination_connection_id
      0x00,  // parameter id
      0x00,  // length
      // max_idle_timeout
      0x01,  // parameter id
      0x02,  // length
      0x6e, 0xec,  // value
      // stateless_reset_token
      0x02,  // parameter id
      0x10,  // length
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
  };
  // clang-format on
  const uint8_t* server_params =
      reinterpret_cast<const uint8_t*>(kServerParamsEmptyOriginalConnectionId);
  size_t server_params_length =
      ABSL_ARRAYSIZE(kServerParamsEmptyOriginalConnectionId);
  TransportParameters out_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_SERVER,
                                       server_params, server_params_length,
                                       &out_params, &error_details))
      << error_details;
  ASSERT_TRUE(out_params.original_destination_connection_id.has_value());
  EXPECT_EQ(out_params.original_destination_connection_id.value(),
            EmptyQuicConnectionId());
}

TEST_P(TransportParametersTest, VeryLongCustomParameter) {
  // Ensure we can handle a 70KB custom parameter on both send and receive.
  std::string custom_value(70000, '?');
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.custom_parameters[kCustomParameter1] = custom_value;

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParameters(orig_params, &serialized));

  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                       serialized.data(), serialized.size(),
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());
  RemoveGreaseParameters(&new_params);
  EXPECT_EQ(new_params, orig_params);
}

TEST_P(TransportParametersTest, SerializationOrderIsRandom) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  orig_params.initial_max_data.set_value(kFakeInitialMaxData);
  orig_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal);
  orig_params.initial_max_stream_data_bidi_remote.set_value(
      kFakeInitialMaxStreamDataBidiRemote);
  orig_params.initial_max_stream_data_uni.set_value(
      kFakeInitialMaxStreamDataUni);
  orig_params.initial_max_streams_bidi.set_value(kFakeInitialMaxStreamsBidi);
  orig_params.initial_max_streams_uni.set_value(kFakeInitialMaxStreamsUni);
  orig_params.ack_delay_exponent.set_value(kAckDelayExponentForTest);
  orig_params.max_ack_delay.set_value(kMaxAckDelayForTest);
  orig_params.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
  orig_params.disable_active_migration = kFakeDisableMigration;
  orig_params.reliable_stream_reset = kFakeReliableStreamReset;
  orig_params.active_connection_id_limit.set_value(
      kActiveConnectionIdLimitForTest);
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  orig_params.initial_round_trip_time_us.set_value(kFakeInitialRoundTripTime);
  orig_params.google_connection_options = CreateFakeGoogleConnectionOptions();
  orig_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  orig_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;

  std::vector<uint8_t> first_serialized;
  ASSERT_TRUE(SerializeTransportParameters(orig_params, &first_serialized));
  // Test that a subsequent serialization is different from the first.
  // Run in a loop to avoid a failure in the unlikely event that randomization
  // produces the same result multiple times.
  for (int i = 0; i < 1000; i++) {
    std::vector<uint8_t> serialized;
    ASSERT_TRUE(SerializeTransportParameters(orig_params, &serialized));
    if (serialized != first_serialized) {
      return;
    }
  }
}

TEST_P(TransportParametersTest, Degrease) {
  TransportParameters orig_params;
  orig_params.perspective = Perspective::IS_CLIENT;
  orig_params.legacy_version_information =
      CreateFakeLegacyVersionInformationClient();
  orig_params.version_information = CreateFakeVersionInformation();
  orig_params.max_idle_timeout_ms.set_value(kFakeIdleTimeoutMilliseconds);
  orig_params.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
  orig_params.initial_max_data.set_value(kFakeInitialMaxData);
  orig_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal);
  orig_params.initial_max_stream_data_bidi_remote.set_value(
      kFakeInitialMaxStreamDataBidiRemote);
  orig_params.initial_max_stream_data_uni.set_value(
      kFakeInitialMaxStreamDataUni);
  orig_params.initial_max_streams_bidi.set_value(kFakeInitialMaxStreamsBidi);
  orig_params.initial_max_streams_uni.set_value(kFakeInitialMaxStreamsUni);
  orig_params.ack_delay_exponent.set_value(kAckDelayExponentForTest);
  orig_params.max_ack_delay.set_value(kMaxAckDelayForTest);
  orig_params.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
  orig_params.disable_active_migration = kFakeDisableMigration;
  orig_params.reliable_stream_reset = kFakeReliableStreamReset;
  orig_params.active_connection_id_limit.set_value(
      kActiveConnectionIdLimitForTest);
  orig_params.initial_source_connection_id =
      CreateFakeInitialSourceConnectionId();
  orig_params.initial_round_trip_time_us.set_value(kFakeInitialRoundTripTime);
  std::string google_handshake_message;
  ASSERT_TRUE(absl::HexStringToBytes(kFakeGoogleHandshakeMessage,
                                     &google_handshake_message));
  orig_params.google_handshake_message = std::move(google_handshake_message);
  orig_params.google_connection_options = CreateFakeGoogleConnectionOptions();
  orig_params.custom_parameters[kCustomParameter1] = kCustomParameter1Value;
  orig_params.custom_parameters[kCustomParameter2] = kCustomParameter2Value;

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParameters(orig_params, &serialized));

  TransportParameters new_params;
  std::string error_details;
  ASSERT_TRUE(ParseTransportParameters(version_, Perspective::IS_CLIENT,
                                       serialized.data(), serialized.size(),
                                       &new_params, &error_details))
      << error_details;
  EXPECT_TRUE(error_details.empty());

  // Deserialized parameters have grease added.
  EXPECT_NE(new_params, orig_params);

  DegreaseTransportParameters(new_params);
  EXPECT_EQ(new_params, orig_params);
}

class TransportParametersTicketSerializationTest : public QuicTest {
 protected:
  void SetUp() override {
    original_params_.perspective = Perspective::IS_SERVER;
    original_params_.legacy_version_information =
        CreateFakeLegacyVersionInformationServer();
    original_params_.original_destination_connection_id =
        CreateFakeOriginalDestinationConnectionId();
    original_params_.max_idle_timeout_ms.set_value(
        kFakeIdleTimeoutMilliseconds);
    original_params_.stateless_reset_token = CreateStatelessResetTokenForTest();
    original_params_.max_udp_payload_size.set_value(kMaxPacketSizeForTest);
    original_params_.initial_max_data.set_value(kFakeInitialMaxData);
    original_params_.initial_max_stream_data_bidi_local.set_value(
        kFakeInitialMaxStreamDataBidiLocal);
    original_params_.initial_max_stream_data_bidi_remote.set_value(
        kFakeInitialMaxStreamDataBidiRemote);
    original_params_.initial_max_stream_data_uni.set_value(
        kFakeInitialMaxStreamDataUni);
    original_params_.initial_max_streams_bidi.set_value(
        kFakeInitialMaxStreamsBidi);
    original_params_.initial_max_streams_uni.set_value(
        kFakeInitialMaxStreamsUni);
    original_params_.ack_delay_exponent.set_value(kAckDelayExponentForTest);
    original_params_.max_ack_delay.set_value(kMaxAckDelayForTest);
    original_params_.min_ack_delay_us_draft10 = kMinAckDelayUsForTest;
    original_params_.disable_active_migration = kFakeDisableMigration;
    original_params_.reliable_stream_reset = kFakeReliableStreamReset;
    original_params_.preferred_address = CreateFakePreferredAddress();
    original_params_.active_connection_id_limit.set_value(
        kActiveConnectionIdLimitForTest);
    original_params_.initial_source_connection_id =
        CreateFakeInitialSourceConnectionId();
    original_params_.retry_source_connection_id =
        CreateFakeRetrySourceConnectionId();
    original_params_.google_connection_options =
        CreateFakeGoogleConnectionOptions();

    ASSERT_TRUE(SerializeTransportParametersForTicket(
        original_params_, application_state_, &original_serialized_params_));
  }

  TransportParameters original_params_;
  std::vector<uint8_t> application_state_ = {0, 1};
  std::vector<uint8_t> original_serialized_params_;
};

TEST_F(TransportParametersTicketSerializationTest,
       StatelessResetTokenDoesntChangeOutput) {
  // Test that changing the stateless reset token doesn't change the ticket
  // serialization.
  TransportParameters new_params = original_params_;
  new_params.stateless_reset_token = CreateFakePreferredStatelessResetToken();
  EXPECT_NE(new_params, original_params_);

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParametersForTicket(
      new_params, application_state_, &serialized));
  EXPECT_EQ(original_serialized_params_, serialized);
}

TEST_F(TransportParametersTicketSerializationTest,
       ConnectionIDDoesntChangeOutput) {
  // Changing original destination CID doesn't change serialization.
  TransportParameters new_params = original_params_;
  new_params.original_destination_connection_id = TestConnectionId(0xCAFE);
  EXPECT_NE(new_params, original_params_);

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParametersForTicket(
      new_params, application_state_, &serialized));
  EXPECT_EQ(original_serialized_params_, serialized);
}

TEST_F(TransportParametersTicketSerializationTest, StreamLimitChangesOutput) {
  // Changing a stream limit does change the serialization.
  TransportParameters new_params = original_params_;
  new_params.initial_max_stream_data_bidi_local.set_value(
      kFakeInitialMaxStreamDataBidiLocal + 1);
  EXPECT_NE(new_params, original_params_);

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParametersForTicket(
      new_params, application_state_, &serialized));
  EXPECT_NE(original_serialized_params_, serialized);
}

TEST_F(TransportParametersTicketSerializationTest,
       ApplicationStateChangesOutput) {
  // Changing the application state changes the serialization.
  std::vector<uint8_t> new_application_state = {0};
  EXPECT_NE(new_application_state, application_state_);

  std::vector<uint8_t> serialized;
  ASSERT_TRUE(SerializeTransportParametersForTicket(
      original_params_, new_application_state, &serialized));
  EXPECT_NE(original_serialized_params_, serialized);
}

void ParseTransportParametersDoesNotCrash(ParsedQuicVersion version,
                                          Perspective perspective,
                                          const std::vector<uint8_t>& data) {
  TransportParameters params;
  std::string error_details;
  (void)ParseTransportParameters(version, perspective, data.data(), data.size(),
                                 &params, &error_details);
}

FUZZ_TEST(TransportParametersFuzzTest, ParseTransportParametersDoesNotCrash)
    .WithDomains(fuzztest::ElementOf(AllSupportedVersions()),
                 fuzztest::ElementOf({Perspective::IS_CLIENT,
                                      Perspective::IS_SERVER}),
                 fuzztest::Arbitrary<std::vector<uint8_t>>());

}  // namespace
}  // namespace test
}  // namespace quic
