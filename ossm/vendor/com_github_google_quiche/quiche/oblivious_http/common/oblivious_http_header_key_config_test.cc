#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_writer.h"

namespace quiche {
namespace {
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

/**
 * Build Request header.
 */
std::string BuildHeader(uint8_t key_id, uint16_t kem_id, uint16_t kdf_id,
                        uint16_t aead_id) {
  int buf_len =
      sizeof(key_id) + sizeof(kem_id) + sizeof(kdf_id) + sizeof(aead_id);
  std::string hdr(buf_len, '\0');
  QuicheDataWriter writer(hdr.size(), hdr.data());
  EXPECT_TRUE(writer.WriteUInt8(key_id));
  EXPECT_TRUE(writer.WriteUInt16(kem_id));   // kemID
  EXPECT_TRUE(writer.WriteUInt16(kdf_id));   // kdfID
  EXPECT_TRUE(writer.WriteUInt16(aead_id));  // aeadID
  return hdr;
}

std::string GetSerializedKeyConfig(
    ObliviousHttpKeyConfigs::OhttpKeyConfig& key_config) {
  uint16_t symmetric_algs_length =
      key_config.symmetric_algorithms.size() *
      (sizeof(key_config.symmetric_algorithms.cbegin()->kdf_id) +
       sizeof(key_config.symmetric_algorithms.cbegin()->aead_id));
  int buf_len = sizeof(key_config.key_id) + sizeof(key_config.kem_id) +
                key_config.public_key.size() + sizeof(symmetric_algs_length) +
                symmetric_algs_length;
  std::string ohttp_key(buf_len, '\0');
  QuicheDataWriter writer(ohttp_key.size(), ohttp_key.data());
  EXPECT_TRUE(writer.WriteUInt8(key_config.key_id));
  EXPECT_TRUE(writer.WriteUInt16(key_config.kem_id));
  EXPECT_TRUE(writer.WriteStringPiece(key_config.public_key));
  EXPECT_TRUE(writer.WriteUInt16(symmetric_algs_length));
  for (const auto& symmetric_alg : key_config.symmetric_algorithms) {
    EXPECT_TRUE(writer.WriteUInt16(symmetric_alg.kdf_id));
    EXPECT_TRUE(writer.WriteUInt16(symmetric_alg.aead_id));
  }
  return ohttp_key;
}

TEST(ObliviousHttpHeaderKeyConfig, TestSerializeRecipientContextInfo) {
  uint8_t key_id = 3;
  uint16_t kem_id = EVP_HPKE_DHKEM_X25519_HKDF_SHA256;
  uint16_t kdf_id = EVP_HPKE_HKDF_SHA256;
  uint16_t aead_id = EVP_HPKE_AES_256_GCM;
  absl::string_view ohttp_req_label = "message/bhttp request";
  std::string expected(ohttp_req_label);
  uint8_t zero_byte = 0x00;
  int buf_len = ohttp_req_label.size() + sizeof(zero_byte) + sizeof(key_id) +
                sizeof(kem_id) + sizeof(kdf_id) + sizeof(aead_id);
  expected.reserve(buf_len);
  expected.push_back(zero_byte);
  std::string ohttp_cfg(BuildHeader(key_id, kem_id, kdf_id, aead_id));
  expected.insert(expected.end(), ohttp_cfg.begin(), ohttp_cfg.end());
  auto instance =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  ASSERT_TRUE(instance.ok());
  EXPECT_EQ(instance.value().SerializeRecipientContextInfo(), expected);
  EXPECT_THAT(instance->DebugString(), HasSubstr("AES-256-GCM"));
}

TEST(ObliviousHttpHeaderKeyConfig, TestValidKeyConfig) {
  auto valid_key_config = ObliviousHttpHeaderKeyConfig::Create(
      2, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  ASSERT_TRUE(valid_key_config.ok());
  EXPECT_THAT(valid_key_config->DebugString(), HasSubstr("AES-256-GCM"));
}

TEST(ObliviousHttpHeaderKeyConfig, TestInvalidKeyConfig) {
  auto invalid_kem = ObliviousHttpHeaderKeyConfig::Create(
      3, 0, EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  EXPECT_EQ(invalid_kem.status().code(), absl::StatusCode::kInvalidArgument);
  auto invalid_kdf = ObliviousHttpHeaderKeyConfig::Create(
      3, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, 0, EVP_HPKE_AES_256_GCM);
  EXPECT_EQ(invalid_kdf.status().code(), absl::StatusCode::kInvalidArgument);
  auto invalid_aead = ObliviousHttpHeaderKeyConfig::Create(
      3, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256, 0);
  EXPECT_EQ(invalid_kdf.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpHeaderKeyConfig, TestParsingValidHeader) {
  auto instance = ObliviousHttpHeaderKeyConfig::Create(
      5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  ASSERT_TRUE(instance.ok());
  std::string good_hdr(BuildHeader(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                   EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM));
  ASSERT_TRUE(instance.value().ParseOhttpPayloadHeader(good_hdr).ok());
}

TEST(ObliviousHttpHeaderKeyConfig, TestParsingInvalidHeader) {
  auto instance = ObliviousHttpHeaderKeyConfig::Create(
      8, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  ASSERT_TRUE(instance.ok());
  std::string keyid_mismatch_hdr(
      BuildHeader(0, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
                  EVP_HPKE_AES_256_GCM));
  EXPECT_EQ(instance.value().ParseOhttpPayloadHeader(keyid_mismatch_hdr).code(),
            absl::StatusCode::kInvalidArgument);
  std::string invalid_hpke_hdr(BuildHeader(8, 0, 0, 0));
  EXPECT_EQ(instance.value().ParseOhttpPayloadHeader(invalid_hpke_hdr).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpHeaderKeyConfig, TestParsingKeyIdFromObliviousHttpRequest) {
  std::string key_id(sizeof(uint8_t), '\0');
  QuicheDataWriter writer(key_id.size(), key_id.data());
  EXPECT_TRUE(writer.WriteUInt8(99));
  auto parsed_key_id =
      ObliviousHttpHeaderKeyConfig::ParseKeyIdFromObliviousHttpRequestPayload(
          key_id);
  ASSERT_TRUE(parsed_key_id.ok());
  EXPECT_EQ(parsed_key_id.value(), 99);
}

TEST(ObliviousHttpHeaderKeyConfig, TestCopyable) {
  auto obj1 = ObliviousHttpHeaderKeyConfig::Create(
      4, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM);
  ASSERT_TRUE(obj1.ok());
  auto copy_obj1_to_obj2 = obj1.value();
  EXPECT_EQ(copy_obj1_to_obj2.kHeaderLength, obj1->kHeaderLength);
  EXPECT_EQ(copy_obj1_to_obj2.SerializeRecipientContextInfo(),
            obj1->SerializeRecipientContextInfo());
}

TEST(ObliviousHttpHeaderKeyConfig, TestSerializeOhttpPayloadHeader) {
  auto instance = ObliviousHttpHeaderKeyConfig::Create(
      7, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_128_GCM);
  ASSERT_TRUE(instance.ok());
  EXPECT_EQ(instance->SerializeOhttpPayloadHeader(),
            BuildHeader(7, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM));
  EXPECT_THAT(instance->DebugString(), HasSubstr("SHA256"));
}

MATCHER_P(HasKeyId, id, "") {
  *result_listener << "has key_id=" << arg.GetKeyId();
  return arg.GetKeyId() == id;
}
MATCHER_P(HasKemId, id, "") {
  *result_listener << "has kem_id=" << arg.GetHpkeKemId();
  return arg.GetHpkeKemId() == id;
}
MATCHER_P(HasKdfId, id, "") {
  *result_listener << "has kdf_id=" << arg.GetHpkeKdfId();
  return arg.GetHpkeKdfId() == id;
}
MATCHER_P(HasAeadId, id, "") {
  *result_listener << "has aead_id=" << arg.GetHpkeAeadId();
  return arg.GetHpkeAeadId() == id;
}

TEST(ObliviousHttpKeyConfigs, SingleKeyConfig) {
  std::string key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "4b0020f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fa27a049bc746a6e97a1e0244b00"
      "0400010002",
      &key));
  auto configs = ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key).value();
  EXPECT_THAT(configs, Property(&ObliviousHttpKeyConfigs::NumKeys, 1));
  EXPECT_THAT(
      configs.PreferredConfig(),
      AllOf(HasKeyId(0x4b), HasKemId(EVP_HPKE_DHKEM_X25519_HKDF_SHA256),
            HasKdfId(EVP_HPKE_HKDF_SHA256), HasAeadId(EVP_HPKE_AES_256_GCM)));
  std::string expected_public_key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fa27a049bc746a6e97a1e0244b",
      &expected_public_key));
  EXPECT_THAT(
      configs.GetPublicKeyForId(configs.PreferredConfig().GetKeyId()).value(),
      StrEq(expected_public_key));
}

TEST(ObliviousHttpKeyConfigs, TwoSimilarKeyConfigs) {
  std::string key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "4b0020f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fa27a049bc746a6e97a1e0244b00"
      "0400010002"  // Intentional concatenation
      "4f0020f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fa27a049bc746a6e97a1e0244b00"
      "0400010001",
      &key));
  EXPECT_THAT(ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key).value(),
              Property(&ObliviousHttpKeyConfigs::NumKeys, 2));
  EXPECT_THAT(
      ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key)->PreferredConfig(),
      AllOf(HasKeyId(0x4f), HasKemId(EVP_HPKE_DHKEM_X25519_HKDF_SHA256),
            HasKdfId(EVP_HPKE_HKDF_SHA256), HasAeadId(EVP_HPKE_AES_128_GCM)));
}

TEST(ObliviousHttpKeyConfigs, RFCExample) {
  std::string key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "01002031e1f05a740102115220e9af918f738674aec95f54db6e04eb705aae8e79815500"
      "080001000100010003",
      &key));
  auto configs = ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key).value();
  EXPECT_THAT(configs, Property(&ObliviousHttpKeyConfigs::NumKeys, 1));
  EXPECT_THAT(
      configs.PreferredConfig(),
      AllOf(HasKeyId(0x01), HasKemId(EVP_HPKE_DHKEM_X25519_HKDF_SHA256),
            HasKdfId(EVP_HPKE_HKDF_SHA256), HasAeadId(EVP_HPKE_AES_128_GCM)));
  std::string expected_public_key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "31e1f05a740102115220e9af918f738674aec95f54db6e04eb705aae8e798155",
      &expected_public_key));
  EXPECT_THAT(
      configs.GetPublicKeyForId(configs.PreferredConfig().GetKeyId()).value(),
      StrEq(expected_public_key));
  EXPECT_THAT(configs.DebugString(), HasSubstr("AES-128-GCM"));
  EXPECT_THAT(configs.DebugString(), HasSubstr("31e1f05a7401"));
}

TEST(ObliviousHttpKeyConfigs, DuplicateKeyId) {
  std::string key;
  ASSERT_TRUE(absl::HexStringToBytes(
      "4b0020f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fa27a049bc746a6e97a1e0244b00"
      "0400010002"  // Intentional concatenation
      "4b0020f83e0a17cbdb18d2684dd2a9b087a43e5f3fa3fb27a049bc746a6e97a1e0244b00"
      "0400010001",
      &key));
  EXPECT_FALSE(ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key).ok());
}

TEST(ObliviousHttpHeaderKeyConfigs, TestCreateWithSingleKeyConfig) {
  auto instance = ObliviousHttpHeaderKeyConfig::Create(
      123, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_CHACHA20_POLY1305);
  ASSERT_TRUE(instance.ok());
  EXPECT_THAT(instance->DebugString(), HasSubstr("CHACHA20-POLY1305"));
  std::string test_public_key(
      EVP_HPKE_KEM_public_key_len(instance->GetHpkeKem()), 'a');
  auto configs =
      ObliviousHttpKeyConfigs::Create(instance.value(), test_public_key);
  EXPECT_TRUE(configs.ok());
  auto serialized_key = configs->GenerateConcatenatedKeys();
  EXPECT_TRUE(serialized_key.ok());
  auto ohttp_configs =
      ObliviousHttpKeyConfigs::ParseConcatenatedKeys(serialized_key.value());
  EXPECT_TRUE(ohttp_configs.ok());
  ASSERT_EQ(ohttp_configs->PreferredConfig().GetKeyId(), 123);
  auto parsed_public_key = ohttp_configs->GetPublicKeyForId(123);
  EXPECT_TRUE(parsed_public_key.ok());
  EXPECT_EQ(parsed_public_key.value(), test_public_key);
}

TEST(ObliviousHttpHeaderKeyConfigs, TestCreateWithWithMultipleKeys) {
  std::string expected_preferred_public_key(32, 'b');
  ObliviousHttpKeyConfigs::OhttpKeyConfig config1 = {
      100,
      EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
      std::string(32, 'a'),
      {{EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM}}};
  EXPECT_THAT(config1.DebugString(), HasSubstr("AES-256-GCM"));
  ObliviousHttpKeyConfigs::OhttpKeyConfig config2 = {
      200,
      EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
      expected_preferred_public_key,
      {{EVP_HPKE_HKDF_SHA256, EVP_HPKE_CHACHA20_POLY1305}}};
  EXPECT_THAT(config2.DebugString(), HasSubstr("CHACHA20-POLY1305"));
  auto configs = ObliviousHttpKeyConfigs::Create({config1, config2});
  ASSERT_TRUE(configs.ok());
  EXPECT_THAT(configs->DebugString(), HasSubstr("CHACHA20-POLY1305"));
  auto serialized_key = configs->GenerateConcatenatedKeys();
  EXPECT_TRUE(serialized_key.ok());
  ASSERT_EQ(serialized_key.value(),
            absl::StrCat(GetSerializedKeyConfig(config2),
                         GetSerializedKeyConfig(config1)));
  auto ohttp_configs =
      ObliviousHttpKeyConfigs::ParseConcatenatedKeys(serialized_key.value());
  ASSERT_TRUE(ohttp_configs.ok());
  EXPECT_THAT(ohttp_configs->DebugString(), HasSubstr("CHACHA20-POLY1305"));
  ASSERT_EQ(ohttp_configs->NumKeys(), 2);
  EXPECT_THAT(configs->PreferredConfig(),
              AllOf(HasKeyId(200), HasKemId(EVP_HPKE_DHKEM_X25519_HKDF_SHA256),
                    HasKdfId(EVP_HPKE_HKDF_SHA256),
                    HasAeadId(EVP_HPKE_CHACHA20_POLY1305)));
  auto parsed_preferred_public_key = ohttp_configs->GetPublicKeyForId(
      ohttp_configs->PreferredConfig().GetKeyId());
  EXPECT_TRUE(parsed_preferred_public_key.ok());
  EXPECT_EQ(parsed_preferred_public_key.value(), expected_preferred_public_key);
}

TEST(ObliviousHttpHeaderKeyConfigs, TestCreateWithInvalidConfigs) {
  ASSERT_EQ(ObliviousHttpKeyConfigs::Create({}).status().code(),
            absl::StatusCode::kInvalidArgument);
  ASSERT_EQ(ObliviousHttpKeyConfigs::Create(
                {{100, 2, std::string(32, 'a'), {{2, 3}, {4, 5}}},
                 {200, 6, std::string(32, 'b'), {{7, 8}, {9, 10}}}})
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);

  EXPECT_EQ(
      ObliviousHttpKeyConfigs::Create(
          {{123,
            EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
            "invalid key length" /*expected length for given kem_id is 32*/,
            {{EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM}}}})
          .status()
          .code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpHeaderKeyConfigs,
     TestCreateSingleKeyConfigWithInvalidConfig) {
  const auto sample_ohttp_hdr_config = ObliviousHttpHeaderKeyConfig::Create(
      123, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_128_GCM);
  ASSERT_TRUE(sample_ohttp_hdr_config.ok());
  EXPECT_THAT(sample_ohttp_hdr_config->DebugString(), HasSubstr("AES-128-GCM"));
  ASSERT_EQ(ObliviousHttpKeyConfigs::Create(sample_ohttp_hdr_config.value(),
                                            "" /*empty public_key*/)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(ObliviousHttpKeyConfigs::Create(
                sample_ohttp_hdr_config.value(),
                "invalid key length" /*expected length for given kem_id is 32*/)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpHeaderKeyConfigs, TestHashImplWithObliviousStruct) {
  // Insert different symmetric algorithms 50 times.
  absl::flat_hash_set<ObliviousHttpKeyConfigs::SymmetricAlgorithmsConfig>
      symmetric_algs_set;
  for (int i = 0; i < 50; ++i) {
    symmetric_algs_set.insert({EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM});
    symmetric_algs_set.insert({EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM});
    symmetric_algs_set.insert(
        {EVP_HPKE_HKDF_SHA256, EVP_HPKE_CHACHA20_POLY1305});
  }
  ASSERT_EQ(symmetric_algs_set.size(), 3);
  EXPECT_THAT(symmetric_algs_set,
              UnorderedElementsAreArray<
                  ObliviousHttpKeyConfigs::SymmetricAlgorithmsConfig>({
                  {EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM},
                  {EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM},
                  {EVP_HPKE_HKDF_SHA256, EVP_HPKE_CHACHA20_POLY1305},
              }));

  // Insert different Key configs 50 times.
  absl::flat_hash_set<ObliviousHttpKeyConfigs::OhttpKeyConfig>
      ohttp_key_configs_set;
  ObliviousHttpKeyConfigs::OhttpKeyConfig expected_key_config{
      100,
      EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
      std::string(32, 'c'),
      {{EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM},
       {EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM}}};
  for (int i = 0; i < 50; ++i) {
    ohttp_key_configs_set.insert(expected_key_config);
  }
  ASSERT_EQ(ohttp_key_configs_set.size(), 1);
  EXPECT_THAT(ohttp_key_configs_set, UnorderedElementsAre(expected_key_config));
}

}  // namespace
}  // namespace quiche
