#include "quiche/oblivious_http/buffers/oblivious_http_request.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hkdf.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace quiche {

namespace {
const uint32_t kHeaderLength = ObliviousHttpHeaderKeyConfig::kHeaderLength;
std::string GetHpkePrivateKey() {
  absl::string_view hpke_key_hex =
      "b77431ecfa8f4cfc30d6e467aafa06944dffe28cb9dd1409e33a3045f5adc8a1";
  std::string hpke_key_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(hpke_key_hex, &hpke_key_bytes));
  return hpke_key_bytes;
}

std::string GetHpkePublicKey() {
  absl::string_view public_key =
      "6d21cfe09fbea5122f9ebc2eb2a69fcc4f06408cd54aac934f012e76fcdcef62";
  std::string public_key_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(public_key, &public_key_bytes));
  return public_key_bytes;
}

std::string GetAlternativeHpkePublicKey() {
  absl::string_view public_key =
      "6d21cfe09fbea5122f9ebc2eb2a69fcc4f06408cd54aac934f012e76fcdcef63";
  std::string public_key_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(public_key, &public_key_bytes));
  return public_key_bytes;
}

std::string GetSeed() {
  absl::string_view seed =
      "52c4a758a802cd8b936eceea314432798d5baf2d7e9235dc084ab1b9cfa2f736";
  std::string seed_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(seed, &seed_bytes));
  return seed_bytes;
}

std::string GetSeededEncapsulatedKey() {
  absl::string_view encapsulated_key =
      "37fda3567bdbd628e88668c3c8d7e97d1d1253b6d4ea6d44c150f741f1bf4431";
  std::string encapsulated_key_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(encapsulated_key, &encapsulated_key_bytes));
  return encapsulated_key_bytes;
}

bssl::UniquePtr<EVP_HPKE_KEY> ConstructHpkeKey(
    absl::string_view hpke_key,
    const ObliviousHttpHeaderKeyConfig &ohttp_key_config) {
  bssl::UniquePtr<EVP_HPKE_KEY> bssl_hpke_key(EVP_HPKE_KEY_new());
  EXPECT_NE(bssl_hpke_key, nullptr);
  EXPECT_TRUE(EVP_HPKE_KEY_init(
      bssl_hpke_key.get(), ohttp_key_config.GetHpkeKem(),
      reinterpret_cast<const uint8_t *>(hpke_key.data()), hpke_key.size()));
  return bssl_hpke_key;
}

const ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                     uint16_t kem_id,
                                                     uint16_t kdf_id,
                                                     uint16_t aead_id) {
  auto ohttp_key_config =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return std::move(ohttp_key_config.value());
}
}  // namespace

// Direct test example from RFC.
// https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A
TEST(ObliviousHttpRequest, TestDecapsulateWithSpecAppendixAExample) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(/*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM);

  // X25519 Secret key (priv key).
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-2
  constexpr absl::string_view kX25519SecretKey =
      "3c168975674b2fa8e465970b79c8dcf09f1c741626480bd4c6162fc5b6a98e1a";

  // Encapsulated request.
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-14
  constexpr absl::string_view kEncapsulatedRequest =
      "010020000100014b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c2"
      "08b4726374e469135906992e1268c594d2a10c695d858c40a026e7965e7d86b83dd440b2"
      "c0185204b4d63525";

  // Initialize Request obj to Decapsulate (decrypt).
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedRequest,
                                     &encapsulated_request_bytes));
  std::string x25519_secret_key_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes(kX25519SecretKey, &x25519_secret_key_bytes));
  auto instance = ObliviousHttpRequest::CreateServerObliviousRequest(
      encapsulated_request_bytes,
      *(ConstructHpkeKey(x25519_secret_key_bytes, ohttp_key_config)),
      ohttp_key_config);
  ASSERT_TRUE(instance.ok());
  auto decrypted = instance->GetPlaintextData();

  // Encapsulated/Ephemeral public key.
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-10
  constexpr absl::string_view kExpectedEphemeralPublicKey =
      "4b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c208b472";
  std::string expected_ephemeral_public_key_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kExpectedEphemeralPublicKey,
                                     &expected_ephemeral_public_key_bytes));
  auto oblivious_request_context = std::move(instance.value()).ReleaseContext();
  EXPECT_EQ(oblivious_request_context.encapsulated_key_,
            expected_ephemeral_public_key_bytes);

  // Binary HTTP message.
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-6
  constexpr absl::string_view kExpectedBinaryHTTPMessage =
      "00034745540568747470730b6578616d706c652e636f6d012f";
  std::string expected_binary_http_message_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kExpectedBinaryHTTPMessage,
                                     &expected_binary_http_message_bytes));
  EXPECT_EQ(decrypted, expected_binary_http_message_bytes);
}

TEST(ObliviousHttpRequest, TestEncapsulatedRequestStructure) {
  uint8_t test_key_id = 7;
  uint16_t test_kem_id = EVP_HPKE_DHKEM_X25519_HKDF_SHA256;
  uint16_t test_kdf_id = EVP_HPKE_HKDF_SHA256;
  uint16_t test_aead_id = EVP_HPKE_AES_256_GCM;
  std::string plaintext = "test";
  auto instance = ObliviousHttpRequest::CreateClientObliviousRequest(
      plaintext, GetHpkePublicKey(),
      GetOhttpKeyConfig(test_key_id, test_kem_id, test_kdf_id, test_aead_id));
  ASSERT_TRUE(instance.ok());
  auto payload_bytes = instance->EncapsulateAndSerialize();
  EXPECT_GE(payload_bytes.size(), kHeaderLength);
  // Parse header.
  QuicheDataReader reader(payload_bytes);
  uint8_t key_id;
  EXPECT_TRUE(reader.ReadUInt8(&key_id));
  EXPECT_EQ(key_id, test_key_id);
  uint16_t kem_id;
  EXPECT_TRUE(reader.ReadUInt16(&kem_id));
  EXPECT_EQ(kem_id, test_kem_id);
  uint16_t kdf_id;
  EXPECT_TRUE(reader.ReadUInt16(&kdf_id));
  EXPECT_EQ(kdf_id, test_kdf_id);
  uint16_t aead_id;
  EXPECT_TRUE(reader.ReadUInt16(&aead_id));
  EXPECT_EQ(aead_id, test_aead_id);
  auto client_request_context = std::move(instance.value()).ReleaseContext();
  auto client_encapsulated_key = client_request_context.encapsulated_key_;
  EXPECT_EQ(client_encapsulated_key.size(), X25519_PUBLIC_VALUE_LEN);
  auto enc_key_plus_ciphertext = payload_bytes.substr(kHeaderLength);
  auto packed_encapsulated_key =
      enc_key_plus_ciphertext.substr(0, X25519_PUBLIC_VALUE_LEN);
  EXPECT_EQ(packed_encapsulated_key, client_encapsulated_key);
  auto ciphertext = enc_key_plus_ciphertext.substr(X25519_PUBLIC_VALUE_LEN);
  EXPECT_GE(ciphertext.size(), plaintext.size());
}

TEST(ObliviousHttpRequest, TestDeterministicSeededOhttpRequest) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(4, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto encapsulated = ObliviousHttpRequest::CreateClientWithSeedForTesting(
      "test", GetHpkePublicKey(), ohttp_key_config, GetSeed());
  ASSERT_TRUE(encapsulated.ok());
  auto encapsulated_request = encapsulated->EncapsulateAndSerialize();
  auto ohttp_request_context = std::move(encapsulated.value()).ReleaseContext();
  EXPECT_EQ(ohttp_request_context.encapsulated_key_,
            GetSeededEncapsulatedKey());
  absl::string_view expected_encrypted_request =
      "9f37cfed07d0111ecd2c34f794671759bcbd922a";
  std::string expected_encrypted_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(expected_encrypted_request,
                                     &expected_encrypted_request_bytes));
  EXPECT_NE(ohttp_request_context.hpke_context_, nullptr);
  size_t encapsulated_key_len = EVP_HPKE_KEM_enc_len(
      EVP_HPKE_CTX_kem(ohttp_request_context.hpke_context_.get()));
  int encrypted_payload_offset = kHeaderLength + encapsulated_key_len;
  EXPECT_EQ(encapsulated_request.substr(encrypted_payload_offset),
            expected_encrypted_request_bytes);
}

TEST(ObliviousHttpRequest,
     TestSeededEncapsulatedKeySamePlaintextsSameCiphertexts) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(8, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto req_with_same_plaintext_1 =
      ObliviousHttpRequest::CreateClientWithSeedForTesting(
          "same plaintext", GetHpkePublicKey(), ohttp_key_config, GetSeed());
  ASSERT_TRUE(req_with_same_plaintext_1.ok());
  auto ciphertext_1 = req_with_same_plaintext_1->EncapsulateAndSerialize();
  auto req_with_same_plaintext_2 =
      ObliviousHttpRequest::CreateClientWithSeedForTesting(
          "same plaintext", GetHpkePublicKey(), ohttp_key_config, GetSeed());
  ASSERT_TRUE(req_with_same_plaintext_2.ok());
  auto ciphertext_2 = req_with_same_plaintext_2->EncapsulateAndSerialize();
  EXPECT_EQ(ciphertext_1, ciphertext_2);
}

TEST(ObliviousHttpRequest,
     TestSeededEncapsulatedKeyDifferentPlaintextsDifferentCiphertexts) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(8, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto req_with_different_plaintext_1 =
      ObliviousHttpRequest::CreateClientWithSeedForTesting(
          "different 1", GetHpkePublicKey(), ohttp_key_config, GetSeed());
  ASSERT_TRUE(req_with_different_plaintext_1.ok());
  auto ciphertext_1 = req_with_different_plaintext_1->EncapsulateAndSerialize();
  auto req_with_different_plaintext_2 =
      ObliviousHttpRequest::CreateClientWithSeedForTesting(
          "different 2", GetHpkePublicKey(), ohttp_key_config, GetSeed());
  ASSERT_TRUE(req_with_different_plaintext_2.ok());
  auto ciphertext_2 = req_with_different_plaintext_2->EncapsulateAndSerialize();
  EXPECT_NE(ciphertext_1, ciphertext_2);
}

TEST(ObliviousHttpRequest, TestInvalidInputsOnClientSide) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(30, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  // Empty plaintext.
  EXPECT_EQ(ObliviousHttpRequest::CreateClientObliviousRequest(
                /*plaintext_payload*/ "", GetHpkePublicKey(), ohttp_key_config)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
  // Empty HPKE public key.
  EXPECT_EQ(ObliviousHttpRequest::CreateClientObliviousRequest(
                "some plaintext",
                /*hpke_public_key*/ "", ohttp_key_config)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpRequest, TestInvalidInputsOnServerSide) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(4, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  // Empty encrypted payload.
  EXPECT_EQ(ObliviousHttpRequest::CreateServerObliviousRequest(
                /*encrypted_data*/ "",
                *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
                ohttp_key_config)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
  // Empty EVP_HPKE_KEY struct.
  EXPECT_EQ(ObliviousHttpRequest::CreateServerObliviousRequest(
                absl::StrCat(ohttp_key_config.SerializeOhttpPayloadHeader(),
                             GetSeededEncapsulatedKey(),
                             "9f37cfed07d0111ecd2c34f794671759bcbd922a"),
                /*gateway_key*/ {}, ohttp_key_config)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpRequest, EndToEndTestForRequest) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto encapsulate = ObliviousHttpRequest::CreateClientObliviousRequest(
      "test", GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(encapsulate.ok());
  auto oblivious_request = encapsulate->EncapsulateAndSerialize();
  auto decapsulate = ObliviousHttpRequest::CreateServerObliviousRequest(
      oblivious_request,
      *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
      ohttp_key_config);
  ASSERT_TRUE(decapsulate.ok());
  auto decrypted = decapsulate->GetPlaintextData();
  EXPECT_EQ(decrypted, "test");
}

TEST(ObliviousHttpRequest, EndToEndTestForRequestWithWrongKey) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto encapsulate = ObliviousHttpRequest::CreateClientObliviousRequest(
      "test", GetAlternativeHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(encapsulate.ok());
  auto oblivious_request = encapsulate->EncapsulateAndSerialize();
  auto decapsulate = ObliviousHttpRequest::CreateServerObliviousRequest(
      oblivious_request,
      *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
      ohttp_key_config);
  EXPECT_EQ(decapsulate.status().code(), absl::StatusCode::kInvalidArgument);
}
}  // namespace quiche
