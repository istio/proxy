#include "quiche/oblivious_http/buffers/oblivious_http_response.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace quiche {

namespace {
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

const ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                     uint16_t kem_id,
                                                     uint16_t kdf_id,
                                                     uint16_t aead_id) {
  auto ohttp_key_config =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return ohttp_key_config.value();
}

bssl::UniquePtr<EVP_HPKE_CTX> GetSeededClientContext(uint8_t key_id,
                                                     uint16_t kem_id,
                                                     uint16_t kdf_id,
                                                     uint16_t aead_id) {
  bssl::UniquePtr<EVP_HPKE_CTX> client_ctx(EVP_HPKE_CTX_new());
  std::string encapsulated_key(EVP_HPKE_MAX_ENC_LENGTH, '\0');
  size_t enc_len;
  std::string info = GetOhttpKeyConfig(key_id, kem_id, kdf_id, aead_id)
                         .SerializeRecipientContextInfo();

  EXPECT_TRUE(EVP_HPKE_CTX_setup_sender_with_seed_for_testing(
      client_ctx.get(), reinterpret_cast<uint8_t *>(encapsulated_key.data()),
      &enc_len, encapsulated_key.size(), EVP_hpke_x25519_hkdf_sha256(),
      EVP_hpke_hkdf_sha256(), EVP_hpke_aes_256_gcm(),
      reinterpret_cast<const uint8_t *>(GetHpkePublicKey().data()),
      GetHpkePublicKey().size(), reinterpret_cast<const uint8_t *>(info.data()),
      info.size(), reinterpret_cast<const uint8_t *>(GetSeed().data()),
      GetSeed().size()));
  encapsulated_key.resize(enc_len);
  EXPECT_EQ(encapsulated_key, GetSeededEncapsulatedKey());
  return client_ctx;
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

ObliviousHttpRequest SetUpObliviousHttpContext(uint8_t key_id, uint16_t kem_id,
                                               uint16_t kdf_id,
                                               uint16_t aead_id,
                                               std::string plaintext) {
  auto ohttp_key_config = GetOhttpKeyConfig(key_id, kem_id, kdf_id, aead_id);
  auto client_request_encapsulate =
      ObliviousHttpRequest::CreateClientWithSeedForTesting(
          std::move(plaintext), GetHpkePublicKey(), ohttp_key_config,
          GetSeed());
  EXPECT_TRUE(client_request_encapsulate.ok());
  auto oblivious_request =
      client_request_encapsulate->EncapsulateAndSerialize();
  auto server_request_decapsulate =
      ObliviousHttpRequest::CreateServerObliviousRequest(
          oblivious_request,
          *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
          ohttp_key_config);
  EXPECT_TRUE(server_request_decapsulate.ok());
  return std::move(server_request_decapsulate.value());
}

// QuicheRandom implementation.
// Just fills the buffer with repeated chars that's initialized in seed.
class TestQuicheRandom : public QuicheRandom {
 public:
  TestQuicheRandom(char seed) : seed_(seed) {}
  ~TestQuicheRandom() override {}

  void RandBytes(void *data, size_t len) override { memset(data, seed_, len); }

  uint64_t RandUint64() override {
    uint64_t random_int;
    memset(&random_int, seed_, sizeof(random_int));
    return random_int;
  }

  void InsecureRandBytes(void *data, size_t len) override {
    return RandBytes(data, len);
  }
  uint64_t InsecureRandUint64() override { return RandUint64(); }

 private:
  char seed_;
};

size_t GetResponseNonceLength(const EVP_HPKE_CTX &hpke_context) {
  EXPECT_NE(&hpke_context, nullptr);
  const EVP_AEAD *evp_hpke_aead =
      EVP_HPKE_AEAD_aead(EVP_HPKE_CTX_aead(&hpke_context));
  EXPECT_NE(evp_hpke_aead, nullptr);
  // Nk = [AEAD key len], is determined by BSSL.
  const size_t aead_key_len = EVP_AEAD_key_length(evp_hpke_aead);
  // Nn = [AEAD nonce len], is determined by BSSL.
  const size_t aead_nonce_len = EVP_AEAD_nonce_length(evp_hpke_aead);
  const size_t secret_len = std::max(aead_key_len, aead_nonce_len);
  return secret_len;
}

TEST(ObliviousHttpResponse, TestDecapsulateReceivedResponse) {
  // Construct encrypted payload with plaintext: "test response"
  absl::string_view encrypted_response =
      "39d5b03c02c97e216df444e4681007105974d4df1585aae05e7b53f3ccdb55d51f711d48"
      "eeefbc1a555d6d928e35df33fd23c23846fa7b083e30692f7b";
  std::string encrypted_response_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes(encrypted_response, &encrypted_response_bytes));
  auto oblivious_context =
      SetUpObliviousHttpContext(4, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM,
                                "test")
          .ReleaseContext();
  auto decapsulated = ObliviousHttpResponse::CreateClientObliviousResponse(
      std::move(encrypted_response_bytes), oblivious_context);
  EXPECT_TRUE(decapsulated.ok());
  auto decrypted = decapsulated->GetPlaintextData();
  EXPECT_EQ(decrypted, "test response");
}
}  // namespace

TEST(ObliviousHttpResponse, EndToEndTestForResponse) {
  auto oblivious_ctx = ObliviousHttpRequest::Context(
      GetSeededClientContext(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                             EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM),
      GetSeededEncapsulatedKey());
  auto server_response_encapsulate =
      ObliviousHttpResponse::CreateServerObliviousResponse("test response",
                                                           oblivious_ctx);
  EXPECT_TRUE(server_response_encapsulate.ok());
  auto oblivious_response =
      server_response_encapsulate->EncapsulateAndSerialize();
  auto client_response_encapsulate =
      ObliviousHttpResponse::CreateClientObliviousResponse(oblivious_response,
                                                           oblivious_ctx);
  auto decrypted = client_response_encapsulate->GetPlaintextData();
  EXPECT_EQ(decrypted, "test response");
}

TEST(ObliviousHttpResponse, TestEncapsulateWithQuicheRandom) {
  auto random = TestQuicheRandom('z');
  auto server_seeded_request = SetUpObliviousHttpContext(
      6, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
      EVP_HPKE_AES_256_GCM, "test");
  auto server_request_context =
      std::move(server_seeded_request).ReleaseContext();
  auto server_response_encapsulate =
      ObliviousHttpResponse::CreateServerObliviousResponse(
          "test response", server_request_context,
          ObliviousHttpHeaderKeyConfig::kOhttpResponseLabel, &random);
  EXPECT_TRUE(server_response_encapsulate.ok());
  std::string response_nonce =
      server_response_encapsulate->EncapsulateAndSerialize().substr(
          0, GetResponseNonceLength(*(server_request_context.hpke_context_)));
  EXPECT_EQ(response_nonce,
            std::string(
                GetResponseNonceLength(*(server_request_context.hpke_context_)),
                'z'));
  absl::string_view expected_encrypted_response =
      "2a3271ac4e6a501f51d0264d3dd7d0bc8a06973b58e89c26d6dac06144";
  std::string expected_encrypted_response_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(expected_encrypted_response,
                                     &expected_encrypted_response_bytes));
  EXPECT_EQ(
      server_response_encapsulate->EncapsulateAndSerialize().substr(
          GetResponseNonceLength(*(server_request_context.hpke_context_))),
      expected_encrypted_response_bytes);
}

}  // namespace quiche
