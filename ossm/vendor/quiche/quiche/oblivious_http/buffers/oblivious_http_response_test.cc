#include "quiche/oblivious_http/buffers/oblivious_http_response.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace quiche {

namespace {

// Example from
// https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-06.html#name-example
constexpr absl::string_view kChunkNonce1Hex = "fead854635d2d5527d64f546";
constexpr absl::string_view kEncryptedChunk1Hex =
    "79bf1cc87fa0e2c02de4546945aa3d1e48";
constexpr absl::string_view kChunkNonce2Hex = "fead854635d2d5527d64f547";
constexpr absl::string_view kEncryptedChunk2Hex =
    "b348b5bd4c594c16b6170b07b475845d1f32";
constexpr absl::string_view kChunkNonce3Hex = "fead854635d2d5527d64f544";
constexpr absl::string_view kEncryptedChunk3Hex =
    "ed9d8a796617a5b27265f4d73247f639";

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

ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id, uint16_t kem_id,
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
      client_ctx.get(), reinterpret_cast<uint8_t*>(encapsulated_key.data()),
      &enc_len, encapsulated_key.size(), EVP_hpke_x25519_hkdf_sha256(),
      EVP_hpke_hkdf_sha256(), EVP_hpke_aes_256_gcm(),
      reinterpret_cast<const uint8_t*>(GetHpkePublicKey().data()),
      GetHpkePublicKey().size(), reinterpret_cast<const uint8_t*>(info.data()),
      info.size(), reinterpret_cast<const uint8_t*>(GetSeed().data()),
      GetSeed().size()));
  encapsulated_key.resize(enc_len);
  EXPECT_EQ(encapsulated_key, GetSeededEncapsulatedKey());
  return client_ctx;
}

bssl::UniquePtr<EVP_HPKE_KEY> ConstructHpkeKey(
    absl::string_view hpke_key,
    const ObliviousHttpHeaderKeyConfig& ohttp_key_config) {
  bssl::UniquePtr<EVP_HPKE_KEY> bssl_hpke_key(EVP_HPKE_KEY_new());
  EXPECT_NE(bssl_hpke_key, nullptr);
  EXPECT_TRUE(EVP_HPKE_KEY_init(
      bssl_hpke_key.get(), ohttp_key_config.GetHpkeKem(),
      reinterpret_cast<const uint8_t*>(hpke_key.data()), hpke_key.size()));
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

  void RandBytes(void* data, size_t len) override { memset(data, seed_, len); }

  uint64_t RandUint64() override {
    uint64_t random_int;
    memset(&random_int, seed_, sizeof(random_int));
    return random_int;
  }

  void InsecureRandBytes(void* data, size_t len) override {
    return RandBytes(data, len);
  }
  uint64_t InsecureRandUint64() override { return RandUint64(); }

 private:
  char seed_;
};

size_t GetResponseNonceLength(const EVP_HPKE_CTX& hpke_context) {
  EXPECT_NE(&hpke_context, nullptr);
  const EVP_AEAD* evp_hpke_aead =
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

struct EncryptChunkTestParams {
  ObliviousHttpRequest::Context context;
  ObliviousHttpResponse::CommonAeadParamsResult aead_params;
  ObliviousHttpResponse::AeadContextData aead_context_data;
};

absl::StatusOr<EncryptChunkTestParams> SetUpEncryptChunkTest() {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-05.html#appendix-A
  auto ohttp_key_config =
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM);

  std::string kX25519SecretKey =
      "1c190d72acdbe4dbc69e680503bb781a932c70a12c8f3754434c67d8640d8698";
  std::string x25519_secret_key_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(kX25519SecretKey, &x25519_secret_key_bytes));
  auto hpke_key = ConstructHpkeKey(x25519_secret_key_bytes, ohttp_key_config);

  std::string encapsulated_request_headers =
      "01002000010001"
      "8811eb457e100811c40a0aa71340a1b81d804bb986f736f2f566a7199761a032";
  std::string encapsulated_request_headers_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(encapsulated_request_headers,
                                     &encapsulated_request_headers_bytes));
  QuicheDataReader reader(encapsulated_request_headers_bytes);

  auto context = ObliviousHttpRequest::DecodeEncapsulatedRequestHeader(
      reader, *hpke_key, ohttp_key_config,
      ObliviousHttpHeaderKeyConfig::kChunkedOhttpRequestLabel);
  QUICHE_EXPECT_OK(context);

  absl::StatusOr<ObliviousHttpResponse::CommonAeadParamsResult> aead_params =
      ObliviousHttpResponse::GetCommonAeadParams(*context);
  EXPECT_TRUE(aead_params.ok());

  auto response_nonce = "bcce7f4cb921309ba5d62edf1769ef09";
  std::string response_nonce_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(response_nonce, &response_nonce_bytes));
  auto aead_context_data = ObliviousHttpResponse::GetAeadContextData(
      *context, *aead_params,
      ObliviousHttpHeaderKeyConfig::kChunkedOhttpResponseLabel,
      response_nonce_bytes);
  QUICHE_EXPECT_OK(aead_context_data);

  return EncryptChunkTestParams{
      .context = std::move(*context),
      .aead_params = std::move(*aead_params),
      .aead_context_data = std::move(*aead_context_data)};
}

TEST(ObliviousHttpResponse, TestEncryptChunks) {
  auto test_params = SetUpEncryptChunkTest();
  QUICHE_EXPECT_OK(test_params);
  auto& [context, aead_params, aead_context_data] = *test_params;

  std::string plaintext_payload = "01";
  std::string plaintext_payload_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(plaintext_payload, &plaintext_payload_bytes));
  std::string chunk_nonce_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce1Hex, &chunk_nonce_bytes));

  auto encrypted_chunk = ObliviousHttpResponse::EncryptChunk(
      context, aead_context_data, plaintext_payload_bytes, chunk_nonce_bytes,
      /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(encrypted_chunk);
  std::string encrypted_chunk_hex = absl::BytesToHexString(*encrypted_chunk);
  EXPECT_EQ(encrypted_chunk_hex, kEncryptedChunk1Hex);

  plaintext_payload = "40c8";
  EXPECT_TRUE(
      absl::HexStringToBytes(plaintext_payload, &plaintext_payload_bytes));
  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce2Hex, &chunk_nonce_bytes));

  encrypted_chunk = ObliviousHttpResponse::EncryptChunk(
      context, aead_context_data, plaintext_payload_bytes, chunk_nonce_bytes,
      /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(encrypted_chunk);
  encrypted_chunk_hex = absl::BytesToHexString(*encrypted_chunk);
  EXPECT_EQ(encrypted_chunk_hex, kEncryptedChunk2Hex);

  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce3Hex, &chunk_nonce_bytes));

  encrypted_chunk = ObliviousHttpResponse::EncryptChunk(
      context, aead_context_data, /*plaintext_payload=*/"", chunk_nonce_bytes,
      /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(encrypted_chunk);
  encrypted_chunk_hex = absl::BytesToHexString(*encrypted_chunk);
  EXPECT_EQ(encrypted_chunk_hex, kEncryptedChunk3Hex);
}

TEST(ObliviousHttpResponse, TestDecryptChunks) {
  absl::StatusOr<EncryptChunkTestParams> test_params = SetUpEncryptChunkTest();
  QUICHE_EXPECT_OK(test_params);
  if (!test_params.ok()) {
    return;
  }
  auto& [context, aead_params, aead_context_data] = *test_params;

  // Chunk 1 decryption
  std::string encrypted_chunk1_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(kEncryptedChunk1Hex, &encrypted_chunk1_bytes));
  std::string chunk_nonce1_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce1Hex, &chunk_nonce1_bytes));
  auto decrypted_chunk1 = ObliviousHttpResponse::DecryptChunk(
      encrypted_chunk1_bytes, aead_context_data, chunk_nonce1_bytes,
      /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(decrypted_chunk1);
  if (!decrypted_chunk1.ok()) {
    return;
  }
  std::string expected_plaintext1_hex = "01";
  std::string expected_plaintext1_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(expected_plaintext1_hex,
                                     &expected_plaintext1_bytes));
  EXPECT_EQ(*decrypted_chunk1, expected_plaintext1_bytes);

  // Chunk 2 decryption
  std::string encrypted_chunk2_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(kEncryptedChunk2Hex, &encrypted_chunk2_bytes));
  std::string chunk_nonce2_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce2Hex, &chunk_nonce2_bytes));
  auto decrypted_chunk2 = ObliviousHttpResponse::DecryptChunk(
      encrypted_chunk2_bytes, aead_context_data, chunk_nonce2_bytes,
      /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(decrypted_chunk2);
  if (!decrypted_chunk2.ok()) {
    return;
  }
  std::string expected_plaintext2_hex = "40c8";
  std::string expected_plaintext2_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(expected_plaintext2_hex,
                                     &expected_plaintext2_bytes));
  EXPECT_EQ(*decrypted_chunk2, expected_plaintext2_bytes);

  // Chunk 3 decryption
  std::string encrypted_chunk3_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(kEncryptedChunk3Hex, &encrypted_chunk3_bytes));
  std::string chunk_nonce3_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(kChunkNonce3Hex, &chunk_nonce3_bytes));
  auto decrypted_chunk3 = ObliviousHttpResponse::DecryptChunk(
      encrypted_chunk3_bytes, aead_context_data, chunk_nonce3_bytes,
      /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(decrypted_chunk3);
  if (!decrypted_chunk3.ok()) {
    return;
  }
  EXPECT_EQ(*decrypted_chunk3, "");
}

TEST(OblviousHttpResponse, EncryptNonFinalChunkWithEmptyPayloadError) {
  auto test_params = SetUpEncryptChunkTest();
  QUICHE_EXPECT_OK(test_params);
  auto& [context, aead_params, aead_context_data] = *test_params;

  EXPECT_EQ(ObliviousHttpResponse::EncryptChunk(context, aead_context_data,
                                                /*plaintext_payload=*/"", "",
                                                /*is_final_chunk=*/false)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(OblviousHttpResponse, EncryptChunkWithEmptyNonceError) {
  auto test_params = SetUpEncryptChunkTest();
  QUICHE_EXPECT_OK(test_params);
  auto& [context, aead_params, aead_context_data] = *test_params;

  EXPECT_EQ(ObliviousHttpResponse::EncryptChunk(context, aead_context_data,
                                                /*plaintext_payload=*/"111", "",
                                                /*is_final_chunk=*/false)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkCounter, EmptyNonceIsInvalid) {
  EXPECT_EQ(ObliviousHttpResponse::ChunkCounter::Create("").status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkCounter, GetChunkNonce) {
  // Chunk nonces from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-05.html#appendix-A
  std::string nonce_hex = "fead854635d2d5527d64f546";
  std::string nonce;
  EXPECT_TRUE(absl::HexStringToBytes(nonce_hex, &nonce));
  auto chunk_counter = ObliviousHttpResponse::ChunkCounter::Create(nonce);
  EXPECT_TRUE(chunk_counter.ok());

  std::string expected_chunk_nonce_hex = "fead854635d2d5527d64f546";
  std::string chunk_nonce;
  EXPECT_TRUE(absl::HexStringToBytes(expected_chunk_nonce_hex, &chunk_nonce));
  EXPECT_EQ(chunk_counter->GetChunkNonce(), chunk_nonce);

  chunk_counter->Increment();
  expected_chunk_nonce_hex = "fead854635d2d5527d64f547";
  EXPECT_TRUE(absl::HexStringToBytes(expected_chunk_nonce_hex, &chunk_nonce));
  EXPECT_EQ(chunk_counter->GetChunkNonce(), chunk_nonce);

  chunk_counter->Increment();
  expected_chunk_nonce_hex = "fead854635d2d5527d64f544";
  EXPECT_TRUE(absl::HexStringToBytes(expected_chunk_nonce_hex, &chunk_nonce));
  EXPECT_EQ(chunk_counter->GetChunkNonce(), chunk_nonce);
}

TEST(ChunkCounter, LimitExceeded) {
  std::string nonce_hex = "00";
  std::string nonce;
  EXPECT_TRUE(absl::HexStringToBytes(nonce_hex, &nonce));
  auto chunk_counter = ObliviousHttpResponse::ChunkCounter::Create(nonce);
  EXPECT_TRUE(chunk_counter.ok());

  for (int i = 0; i < 256; ++i) {
    EXPECT_FALSE(chunk_counter->LimitExceeded());
    chunk_counter->Increment();
  }

  // Counter limit reached at 2^(nonce_size * 8)
  EXPECT_TRUE(chunk_counter->LimitExceeded());
}

}  // namespace quiche
