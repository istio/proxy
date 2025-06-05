// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_versions.h"

#include <cstddef>
#include <sstream>

#include "absl/algorithm/container.h"
#include "absl/base/macros.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(QuicVersionsTest, CreateQuicVersionLabelUnsupported) {
  EXPECT_QUIC_BUG(
      CreateQuicVersionLabel(UnsupportedQuicVersion()),
      "Unsupported version QUIC_VERSION_UNSUPPORTED PROTOCOL_UNSUPPORTED");
}

TEST(QuicVersionsTest, KnownAndValid) {
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_TRUE(version.IsKnown());
    EXPECT_TRUE(ParsedQuicVersionIsValid(version.handshake_protocol,
                                         version.transport_version));
  }
  ParsedQuicVersion unsupported = UnsupportedQuicVersion();
  EXPECT_FALSE(unsupported.IsKnown());
  EXPECT_TRUE(ParsedQuicVersionIsValid(unsupported.handshake_protocol,
                                       unsupported.transport_version));
  ParsedQuicVersion reserved = QuicVersionReservedForNegotiation();
  EXPECT_TRUE(reserved.IsKnown());
  EXPECT_TRUE(ParsedQuicVersionIsValid(reserved.handshake_protocol,
                                       reserved.transport_version));
  // Check that invalid combinations are not valid.
  EXPECT_FALSE(ParsedQuicVersionIsValid(PROTOCOL_TLS1_3, QUIC_VERSION_46));
  EXPECT_FALSE(ParsedQuicVersionIsValid(PROTOCOL_QUIC_CRYPTO,
                                        QUIC_VERSION_IETF_DRAFT_29));
  // Check that deprecated versions are not valid.
  EXPECT_FALSE(ParsedQuicVersionIsValid(PROTOCOL_QUIC_CRYPTO,
                                        static_cast<QuicTransportVersion>(33)));
  EXPECT_FALSE(ParsedQuicVersionIsValid(PROTOCOL_QUIC_CRYPTO,
                                        static_cast<QuicTransportVersion>(99)));
  EXPECT_FALSE(ParsedQuicVersionIsValid(PROTOCOL_TLS1_3,
                                        static_cast<QuicTransportVersion>(99)));
}

TEST(QuicVersionsTest, Features) {
  ParsedQuicVersion parsed_version_q046 = ParsedQuicVersion::Q046();
  ParsedQuicVersion parsed_version_draft_29 = ParsedQuicVersion::Draft29();

  EXPECT_TRUE(parsed_version_q046.IsKnown());
  EXPECT_FALSE(parsed_version_q046.KnowsWhichDecrypterToUse());
  EXPECT_FALSE(parsed_version_q046.UsesInitialObfuscators());
  EXPECT_FALSE(parsed_version_q046.AllowsLowFlowControlLimits());
  EXPECT_FALSE(parsed_version_q046.HasHeaderProtection());
  EXPECT_FALSE(parsed_version_q046.SupportsRetry());
  EXPECT_FALSE(
      parsed_version_q046.SendsVariableLengthPacketNumberInLongHeader());
  EXPECT_FALSE(parsed_version_q046.AllowsVariableLengthConnectionIds());
  EXPECT_FALSE(parsed_version_q046.SupportsClientConnectionIds());
  EXPECT_FALSE(parsed_version_q046.HasLengthPrefixedConnectionIds());
  EXPECT_FALSE(parsed_version_q046.SupportsAntiAmplificationLimit());
  EXPECT_FALSE(parsed_version_q046.CanSendCoalescedPackets());
  EXPECT_TRUE(parsed_version_q046.SupportsGoogleAltSvcFormat());
  EXPECT_FALSE(parsed_version_q046.UsesHttp3());
  EXPECT_FALSE(parsed_version_q046.HasLongHeaderLengths());
  EXPECT_FALSE(parsed_version_q046.UsesCryptoFrames());
  EXPECT_FALSE(parsed_version_q046.HasIetfQuicFrames());
  EXPECT_FALSE(parsed_version_q046.UsesTls());
  EXPECT_TRUE(parsed_version_q046.UsesQuicCrypto());

  EXPECT_TRUE(parsed_version_draft_29.IsKnown());
  EXPECT_TRUE(parsed_version_draft_29.KnowsWhichDecrypterToUse());
  EXPECT_TRUE(parsed_version_draft_29.UsesInitialObfuscators());
  EXPECT_TRUE(parsed_version_draft_29.AllowsLowFlowControlLimits());
  EXPECT_TRUE(parsed_version_draft_29.HasHeaderProtection());
  EXPECT_TRUE(parsed_version_draft_29.SupportsRetry());
  EXPECT_TRUE(
      parsed_version_draft_29.SendsVariableLengthPacketNumberInLongHeader());
  EXPECT_TRUE(parsed_version_draft_29.AllowsVariableLengthConnectionIds());
  EXPECT_TRUE(parsed_version_draft_29.SupportsClientConnectionIds());
  EXPECT_TRUE(parsed_version_draft_29.HasLengthPrefixedConnectionIds());
  EXPECT_TRUE(parsed_version_draft_29.SupportsAntiAmplificationLimit());
  EXPECT_TRUE(parsed_version_draft_29.CanSendCoalescedPackets());
  EXPECT_FALSE(parsed_version_draft_29.SupportsGoogleAltSvcFormat());
  EXPECT_TRUE(parsed_version_draft_29.UsesHttp3());
  EXPECT_TRUE(parsed_version_draft_29.HasLongHeaderLengths());
  EXPECT_TRUE(parsed_version_draft_29.UsesCryptoFrames());
  EXPECT_TRUE(parsed_version_draft_29.HasIetfQuicFrames());
  EXPECT_TRUE(parsed_version_draft_29.UsesTls());
  EXPECT_FALSE(parsed_version_draft_29.UsesQuicCrypto());
}

TEST(QuicVersionsTest, ParseQuicVersionLabel) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ(ParsedQuicVersion::Q046(),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '6')));
  EXPECT_EQ(ParsedQuicVersion::Draft29(),
            ParseQuicVersionLabel(MakeVersionLabel(0xff, 0x00, 0x00, 0x1d)));
  EXPECT_EQ(ParsedQuicVersion::RFCv1(),
            ParseQuicVersionLabel(MakeVersionLabel(0x00, 0x00, 0x00, 0x01)));
  EXPECT_EQ(ParsedQuicVersion::RFCv2(),
            ParseQuicVersionLabel(MakeVersionLabel(0x6b, 0x33, 0x43, 0xcf)));
  EXPECT_EQ((ParsedQuicVersionVector{ParsedQuicVersion::RFCv2(),
                                     ParsedQuicVersion::RFCv1(),
                                     ParsedQuicVersion::Draft29()}),
            ParseQuicVersionLabelVector(QuicVersionLabelVector{
                MakeVersionLabel(0x6b, 0x33, 0x43, 0xcf),
                MakeVersionLabel(0x00, 0x00, 0x00, 0x01),
                MakeVersionLabel(0xaa, 0xaa, 0xaa, 0xaa),
                MakeVersionLabel(0xff, 0x00, 0x00, 0x1d)}));

  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_EQ(version, ParseQuicVersionLabel(CreateQuicVersionLabel(version)));
  }
}

TEST(QuicVersionsTest, ParseQuicVersionString) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ(ParsedQuicVersion::Q046(),
            ParseQuicVersionString("QUIC_VERSION_46"));
  EXPECT_EQ(ParsedQuicVersion::Q046(), ParseQuicVersionString("46"));
  EXPECT_EQ(ParsedQuicVersion::Q046(), ParseQuicVersionString("Q046"));

  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionString(""));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionString("Q 46"));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionString("Q046 "));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionString("99"));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionString("70"));

  EXPECT_EQ(ParsedQuicVersion::Draft29(), ParseQuicVersionString("ff00001d"));
  EXPECT_EQ(ParsedQuicVersion::Draft29(), ParseQuicVersionString("draft29"));
  EXPECT_EQ(ParsedQuicVersion::Draft29(), ParseQuicVersionString("h3-29"));

  EXPECT_EQ(ParsedQuicVersion::RFCv1(), ParseQuicVersionString("00000001"));
  EXPECT_EQ(ParsedQuicVersion::RFCv1(), ParseQuicVersionString("h3"));

  // QUICv2 will never be the result for "h3".

  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_EQ(version,
              ParseQuicVersionString(ParsedQuicVersionToString(version)));
    EXPECT_EQ(version, ParseQuicVersionString(QuicVersionLabelToString(
                           CreateQuicVersionLabel(version))));
    if (!version.AlpnDeferToRFCv1()) {
      EXPECT_EQ(version, ParseQuicVersionString(AlpnForVersion(version)));
    }
  }
}

TEST(QuicVersionsTest, ParseQuicVersionVectorString) {
  ParsedQuicVersion version_q046 = ParsedQuicVersion::Q046();
  ParsedQuicVersion version_draft_29 = ParsedQuicVersion::Draft29();

  EXPECT_THAT(ParseQuicVersionVectorString(""), IsEmpty());

  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-Q046"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-Q046, h3-29"),
              ElementsAre(version_q046, version_draft_29));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-29,h3-Q046,h3-29"),
              ElementsAre(version_draft_29, version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-29, h3-Q046"),
              ElementsAre(version_draft_29, version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46,h3-29"),
              ElementsAre(version_q046, version_draft_29));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-29,QUIC_VERSION_46"),
              ElementsAre(version_draft_29, version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46, h3-29"),
              ElementsAre(version_q046, version_draft_29));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-29, QUIC_VERSION_46"),
              ElementsAre(version_draft_29, version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-29,QUIC_VERSION_46"),
              ElementsAre(version_draft_29, version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46,h3-29"),
              ElementsAre(version_q046, version_draft_29));

  // Regression test for https://crbug.com/1044952.
  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46, QUIC_VERSION_46"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-Q046, h3-Q046"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("h3-Q046, QUIC_VERSION_46"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString(
                  "QUIC_VERSION_46, h3-Q046, QUIC_VERSION_46, h3-Q046"),
              ElementsAre(version_q046));
  EXPECT_THAT(ParseQuicVersionVectorString("QUIC_VERSION_46, h3-29, h3-Q046"),
              ElementsAre(version_q046, version_draft_29));

  EXPECT_THAT(ParseQuicVersionVectorString("99"), IsEmpty());
  EXPECT_THAT(ParseQuicVersionVectorString("70"), IsEmpty());
  EXPECT_THAT(ParseQuicVersionVectorString("h3-01"), IsEmpty());
  EXPECT_THAT(ParseQuicVersionVectorString("h3-01,h3-29"),
              ElementsAre(version_draft_29));
}

// Do not use MakeVersionLabel() to generate expectations, because
// CreateQuicVersionLabel() uses MakeVersionLabel() internally,
// in case it has a bug.
TEST(QuicVersionsTest, CreateQuicVersionLabel) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ(0x51303436u, CreateQuicVersionLabel(ParsedQuicVersion::Q046()));
  EXPECT_EQ(0xff00001du, CreateQuicVersionLabel(ParsedQuicVersion::Draft29()));
  EXPECT_EQ(0x00000001u, CreateQuicVersionLabel(ParsedQuicVersion::RFCv1()));
  EXPECT_EQ(0x6b3343cfu, CreateQuicVersionLabel(ParsedQuicVersion::RFCv2()));

  // Make sure the negotiation reserved version is in the IETF reserved space.
  EXPECT_EQ(
      0xda5a3a3au & 0x0f0f0f0f,
      CreateQuicVersionLabel(ParsedQuicVersion::ReservedForNegotiation()) &
          0x0f0f0f0f);

  // Make sure that disabling randomness works.
  SetQuicFlag(quic_disable_version_negotiation_grease_randomness, true);
  EXPECT_EQ(0xda5a3a3au, CreateQuicVersionLabel(
                             ParsedQuicVersion::ReservedForNegotiation()));
}

TEST(QuicVersionsTest, QuicVersionLabelToString) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ("Q046", QuicVersionLabelToString(
                        CreateQuicVersionLabel(ParsedQuicVersion::Q046())));
  EXPECT_EQ("ff00001d", QuicVersionLabelToString(CreateQuicVersionLabel(
                            ParsedQuicVersion::Draft29())));
  EXPECT_EQ("00000001", QuicVersionLabelToString(CreateQuicVersionLabel(
                            ParsedQuicVersion::RFCv1())));
  EXPECT_EQ("6b3343cf", QuicVersionLabelToString(CreateQuicVersionLabel(
                            ParsedQuicVersion::RFCv2())));

  QuicVersionLabelVector version_labels = {
      MakeVersionLabel('Q', '0', '3', '5'),
      MakeVersionLabel('T', '0', '3', '8'),
      MakeVersionLabel(0xff, 0, 0, 7),
  };

  EXPECT_EQ("Q035", QuicVersionLabelToString(version_labels[0]));
  EXPECT_EQ("T038", QuicVersionLabelToString(version_labels[1]));
  EXPECT_EQ("ff000007", QuicVersionLabelToString(version_labels[2]));

  EXPECT_EQ("Q035,T038,ff000007",
            QuicVersionLabelVectorToString(version_labels));
  EXPECT_EQ("Q035:T038:ff000007",
            QuicVersionLabelVectorToString(version_labels, ":", 2));
  EXPECT_EQ("Q035|T038|...",
            QuicVersionLabelVectorToString(version_labels, "|", 1));

  std::ostringstream os;
  os << version_labels;
  EXPECT_EQ("Q035,T038,ff000007", os.str());
}

TEST(QuicVersionsTest, ParseQuicVersionLabelString) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  // Explicitly test known QUIC version label strings.
  EXPECT_EQ(ParsedQuicVersion::Q046(), ParseQuicVersionLabelString("Q046"));
  EXPECT_EQ(ParsedQuicVersion::Draft29(),
            ParseQuicVersionLabelString("ff00001d"));
  EXPECT_EQ(ParsedQuicVersion::RFCv1(),
            ParseQuicVersionLabelString("00000001"));
  EXPECT_EQ(ParsedQuicVersion::RFCv2(),
            ParseQuicVersionLabelString("6b3343cf"));

  // Sanity check that a variety of other serialization formats are ignored.
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionLabelString("1"));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionLabelString("46"));
  EXPECT_EQ(UnsupportedQuicVersion(),
            ParseQuicVersionLabelString("QUIC_VERSION_46"));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionLabelString("h3"));
  EXPECT_EQ(UnsupportedQuicVersion(), ParseQuicVersionLabelString("h3-29"));

  // Test round-trips between QuicVersionLabelToString and
  // ParseQuicVersionLabelString.
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_EQ(version, ParseQuicVersionLabelString(QuicVersionLabelToString(
                           CreateQuicVersionLabel(version))));
  }
}

TEST(QuicVersionsTest, QuicVersionToString) {
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED",
            QuicVersionToString(QUIC_VERSION_UNSUPPORTED));

  QuicTransportVersion single_version[] = {QUIC_VERSION_46};
  QuicTransportVersionVector versions_vector;
  for (size_t i = 0; i < ABSL_ARRAYSIZE(single_version); ++i) {
    versions_vector.push_back(single_version[i]);
  }
  EXPECT_EQ("QUIC_VERSION_46",
            QuicTransportVersionVectorToString(versions_vector));

  QuicTransportVersion multiple_versions[] = {QUIC_VERSION_UNSUPPORTED,
                                              QUIC_VERSION_46};
  versions_vector.clear();
  for (size_t i = 0; i < ABSL_ARRAYSIZE(multiple_versions); ++i) {
    versions_vector.push_back(multiple_versions[i]);
  }
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED,QUIC_VERSION_46",
            QuicTransportVersionVectorToString(versions_vector));

  // Make sure that all supported versions are present in QuicVersionToString.
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_NE("QUIC_VERSION_UNSUPPORTED",
              QuicVersionToString(version.transport_version));
  }

  std::ostringstream os;
  os << versions_vector;
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED,QUIC_VERSION_46", os.str());
}

TEST(QuicVersionsTest, ParsedQuicVersionToString) {
  EXPECT_EQ("0", ParsedQuicVersionToString(ParsedQuicVersion::Unsupported()));
  EXPECT_EQ("Q046", ParsedQuicVersionToString(ParsedQuicVersion::Q046()));
  EXPECT_EQ("draft29", ParsedQuicVersionToString(ParsedQuicVersion::Draft29()));
  EXPECT_EQ("RFCv1", ParsedQuicVersionToString(ParsedQuicVersion::RFCv1()));
  EXPECT_EQ("RFCv2", ParsedQuicVersionToString(ParsedQuicVersion::RFCv2()));

  ParsedQuicVersionVector versions_vector = {ParsedQuicVersion::Q046()};
  EXPECT_EQ("Q046", ParsedQuicVersionVectorToString(versions_vector));

  versions_vector = {ParsedQuicVersion::Unsupported(),
                     ParsedQuicVersion::Q046()};
  EXPECT_EQ("0,Q046", ParsedQuicVersionVectorToString(versions_vector));
  EXPECT_EQ("0:Q046", ParsedQuicVersionVectorToString(versions_vector, ":",
                                                      versions_vector.size()));
  EXPECT_EQ("0|...", ParsedQuicVersionVectorToString(versions_vector, "|", 0));

  // Make sure that all supported versions are present in
  // ParsedQuicVersionToString.
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_NE("0", ParsedQuicVersionToString(version));
  }

  std::ostringstream os;
  os << versions_vector;
  EXPECT_EQ("0,Q046", os.str());
}

TEST(QuicVersionsTest, FilterSupportedVersionsAllVersions) {
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    QuicEnableVersion(version);
  }
  ParsedQuicVersionVector expected_parsed_versions;
  for (const ParsedQuicVersion& version : SupportedVersions()) {
    expected_parsed_versions.push_back(version);
  }
  EXPECT_EQ(expected_parsed_versions,
            FilterSupportedVersions(AllSupportedVersions()));
  EXPECT_EQ(expected_parsed_versions, AllSupportedVersions());
}

TEST(QuicVersionsTest, FilterSupportedVersionsWithoutFirstVersion) {
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    QuicEnableVersion(version);
  }
  QuicDisableVersion(AllSupportedVersions().front());
  ParsedQuicVersionVector expected_parsed_versions;
  for (const ParsedQuicVersion& version : SupportedVersions()) {
    expected_parsed_versions.push_back(version);
  }
  expected_parsed_versions.erase(expected_parsed_versions.begin());
  EXPECT_EQ(expected_parsed_versions,
            FilterSupportedVersions(AllSupportedVersions()));
}

TEST(QuicVersionsTest, LookUpParsedVersionByIndex) {
  ParsedQuicVersionVector all_versions = AllSupportedVersions();
  int version_count = all_versions.size();
  for (int i = -5; i <= version_count + 1; ++i) {
    ParsedQuicVersionVector index = ParsedVersionOfIndex(all_versions, i);
    if (i >= 0 && i < version_count) {
      EXPECT_EQ(all_versions[i], index[0]);
    } else {
      EXPECT_EQ(UnsupportedQuicVersion(), index[0]);
    }
  }
}

// This test may appear to be so simplistic as to be unnecessary,
// yet a typo was made in doing the #defines and it was caught
// only in some test far removed from here... Better safe than sorry.
TEST(QuicVersionsTest, CheckTransportVersionNumbersForTypos) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ(QUIC_VERSION_46, 46);
  EXPECT_EQ(QUIC_VERSION_IETF_DRAFT_29, 73);
  EXPECT_EQ(QUIC_VERSION_IETF_RFC_V1, 80);
  EXPECT_EQ(QUIC_VERSION_IETF_RFC_V2, 82);
}

TEST(QuicVersionsTest, AlpnForVersion) {
  static_assert(SupportedVersions().size() == 4u,
                "Supported versions out of sync");
  EXPECT_EQ("h3-Q046", AlpnForVersion(ParsedQuicVersion::Q046()));
  EXPECT_EQ("h3-29", AlpnForVersion(ParsedQuicVersion::Draft29()));
  EXPECT_EQ("h3", AlpnForVersion(ParsedQuicVersion::RFCv1()));
  EXPECT_EQ("h3", AlpnForVersion(ParsedQuicVersion::RFCv2()));
}

TEST(QuicVersionsTest, QuicVersionEnabling) {
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    QuicFlagSaver flag_saver;
    QuicDisableVersion(version);
    EXPECT_FALSE(QuicVersionIsEnabled(version));
    QuicEnableVersion(version);
    EXPECT_TRUE(QuicVersionIsEnabled(version));
  }
}

TEST(QuicVersionsTest, ReservedForNegotiation) {
  EXPECT_EQ(QUIC_VERSION_RESERVED_FOR_NEGOTIATION,
            QuicVersionReservedForNegotiation().transport_version);
  // QUIC_VERSION_RESERVED_FOR_NEGOTIATION MUST NOT be supported.
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    EXPECT_NE(QUIC_VERSION_RESERVED_FOR_NEGOTIATION, version.transport_version);
  }
}

TEST(QuicVersionsTest, SupportedVersionsHasCorrectList) {
  size_t index = 0;
  for (HandshakeProtocol handshake_protocol : SupportedHandshakeProtocols()) {
    for (int trans_vers = 255; trans_vers > 0; trans_vers--) {
      QuicTransportVersion transport_version =
          static_cast<QuicTransportVersion>(trans_vers);
      SCOPED_TRACE(index);
      if (ParsedQuicVersionIsValid(handshake_protocol, transport_version)) {
        ParsedQuicVersion version = SupportedVersions()[index];
        EXPECT_EQ(version,
                  ParsedQuicVersion(handshake_protocol, transport_version));
        index++;
      }
    }
  }
  EXPECT_EQ(SupportedVersions().size(), index);
}

TEST(QuicVersionsTest, SupportedVersionsAllDistinct) {
  for (size_t index1 = 0; index1 < SupportedVersions().size(); ++index1) {
    ParsedQuicVersion version1 = SupportedVersions()[index1];
    for (size_t index2 = index1 + 1; index2 < SupportedVersions().size();
         ++index2) {
      ParsedQuicVersion version2 = SupportedVersions()[index2];
      EXPECT_NE(version1, version2) << version1 << " " << version2;
      EXPECT_NE(CreateQuicVersionLabel(version1),
                CreateQuicVersionLabel(version2))
          << version1 << " " << version2;
      // The one pair where ALPNs are the same.
      if ((version1 != ParsedQuicVersion::RFCv2()) &&
          (version2 != ParsedQuicVersion::RFCv1())) {
        EXPECT_NE(AlpnForVersion(version1), AlpnForVersion(version2))
            << version1 << " " << version2;
      }
    }
  }
}

TEST(QuicVersionsTest, CurrentSupportedHttp3Versions) {
  ParsedQuicVersionVector h3_versions = CurrentSupportedHttp3Versions();
  ParsedQuicVersionVector all_current_supported_versions =
      CurrentSupportedVersions();
  for (auto& version : all_current_supported_versions) {
    bool version_is_h3 = false;
    for (auto& h3_version : h3_versions) {
      if (version == h3_version) {
        EXPECT_TRUE(version.UsesHttp3());
        version_is_h3 = true;
        break;
      }
    }
    if (!version_is_h3) {
      EXPECT_FALSE(version.UsesHttp3());
    }
  }
}

TEST(QuicVersionsTest, ObsoleteSupportedVersions) {
  ParsedQuicVersionVector obsolete_versions = ObsoleteSupportedVersions();
  EXPECT_EQ(quic::ParsedQuicVersion::Q046(), obsolete_versions[0]);
  EXPECT_EQ(quic::ParsedQuicVersion::Draft29(), obsolete_versions[1]);
}

TEST(QuicVersionsTest, IsObsoleteSupportedVersion) {
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    bool is_obsolete = version.handshake_protocol != PROTOCOL_TLS1_3 ||
                       version.transport_version < QUIC_VERSION_IETF_RFC_V1;
    EXPECT_EQ(is_obsolete, IsObsoleteSupportedVersion(version));
  }
}

TEST(QuicVersionsTest, CurrentSupportedVersionsForClients) {
  ParsedQuicVersionVector supported_versions = CurrentSupportedVersions();
  ParsedQuicVersionVector client_versions =
      CurrentSupportedVersionsForClients();
  for (auto& version : supported_versions) {
    const bool is_obsolete = IsObsoleteSupportedVersion(version);
    const bool is_supported =
        absl::c_find(client_versions, version) != client_versions.end();
    // Every supported version which is not obsolete should be a supported
    // client version.
    EXPECT_EQ(!is_obsolete, is_supported);
  }
  // Every client version should be a supported version, of course.
  for (auto& version : client_versions) {
    EXPECT_TRUE(absl::c_find(supported_versions, version) !=
                supported_versions.end());
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
