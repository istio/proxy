#include "quiche/oblivious_http/oblivious_http_gateway.h"

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_thread.h"
#include "quiche/common/quiche_random.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/common/oblivious_http_chunk_handler.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"

namespace quiche {
namespace {

constexpr absl::string_view kEncapsulatedChunkedRequest =
    "01002000010001"
    "8811eb457e100811c40a0aa71340a1b81d804bb986f736f2f566a7199761a032"
    "1c2ad24942d4d692563012f2980c8fef437a336b9b2fc938ef77a5834f"
    "1d2e33d8fd25577afe31bd1c79d094f76b6250ae6549b473ecd950501311"
    "001c6c1395d0ef7c1022297966307b8a7f";

class TestChunkHandler : public ObliviousHttpChunkHandler {
 public:
  TestChunkHandler() = default;
  ~TestChunkHandler() override = default;
  absl::Status OnDecryptedChunk(absl::string_view decrypted_chunk) override {
    EXPECT_FALSE(on_chunks_done_called_);
    chunk_count_++;
    absl::StrAppend(&concatenated_decrypted_chunks_, decrypted_chunk);
    return absl::OkStatus();
  }
  absl::Status OnChunksDone() override {
    EXPECT_FALSE(on_chunks_done_called_);
    on_chunks_done_called_ = true;
    std::string expected_request;
    EXPECT_TRUE(absl::HexStringToBytes(
        "00034745540568747470730b6578616d706c652e636f6d012f",
        &expected_request));
    EXPECT_EQ(concatenated_decrypted_chunks_, expected_request);
    return absl::OkStatus();
  }
  uint64_t GetChunkCount() const { return chunk_count_; }
  bool GetOnChunksDoneCalled() const { return on_chunks_done_called_; }

 private:
  uint64_t chunk_count_ = 0;
  bool on_chunks_done_called_ = false;
  std::string concatenated_decrypted_chunks_;
};

std::string GetHpkePrivateKey() {
  // Dev/Test private key generated using Keystore.
  absl::string_view hpke_key_hex =
      "b77431ecfa8f4cfc30d6e467aafa06944dffe28cb9dd1409e33a3045f5adc8a1";
  std::string hpke_key_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(hpke_key_hex, &hpke_key_bytes));
  return hpke_key_bytes;
}

std::string GetHpkePublicKey() {
  // Dev/Test public key generated using Keystore.
  absl::string_view public_key =
      "6d21cfe09fbea5122f9ebc2eb2a69fcc4f06408cd54aac934f012e76fcdcef62";
  std::string public_key_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(public_key, &public_key_bytes));
  return public_key_bytes;
}

ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id, uint16_t kem_id,
                                               uint16_t kdf_id,
                                               uint16_t aead_id) {
  auto ohttp_key_config =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return std::move(ohttp_key_config.value());
}

TEST(ObliviousHttpGateway, TestProvisioningKeyAndDecapsulate) {
  // X25519 Secret key (priv key).
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-2
  constexpr absl::string_view kX25519SecretKey =
      "3c168975674b2fa8e465970b79c8dcf09f1c741626480bd4c6162fc5b6a98e1a";
  std::string x25519_secret_key_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes(kX25519SecretKey, &x25519_secret_key_bytes));

  auto instance = ObliviousHttpGateway::Create(
      /*hpke_private_key*/ x25519_secret_key_bytes,
      /*ohttp_key_config*/ GetOhttpKeyConfig(
          /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_128_GCM));

  // Encapsulated request.
  // https://www.rfc-editor.org/rfc/rfc9458.html#appendix-A-14
  constexpr absl::string_view kEncapsulatedRequest =
      "010020000100014b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c2"
      "08b4726374e469135906992e1268c594d2a10c695d858c40a026e7965e7d86b83dd440b2"
      "c0185204b4d63525";
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedRequest,
                                     &encapsulated_request_bytes));

  auto decrypted_req =
      instance->DecryptObliviousHttpRequest(encapsulated_request_bytes);
  ASSERT_TRUE(decrypted_req.ok());
  ASSERT_FALSE(decrypted_req->GetPlaintextData().empty());
}

absl::StatusOr<ChunkedObliviousHttpGateway> CreateChunkedObliviousHttpGateway(
    ObliviousHttpChunkHandler& chunk_handler,
    QuicheRandom* quiche_random = nullptr) {
  constexpr absl::string_view kX25519SecretKey =
      "1c190d72acdbe4dbc69e680503bb781a932c70a12c8f3754434c67d8640d8698";
  std::string x25519_secret_key_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(kX25519SecretKey, &x25519_secret_key_bytes));

  return ChunkedObliviousHttpGateway::Create(
      x25519_secret_key_bytes,
      GetOhttpKeyConfig(
          /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_128_GCM),
      chunk_handler, quiche_random);
}

TEST(ChunkedObliviousHttpGateway, ProvisionKeyAndDecapsulateFullRequest) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-05.html#appendix-A
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));

  QUICHE_EXPECT_OK(instance->DecryptRequest(encapsulated_request_bytes, true));
  EXPECT_TRUE(chunk_handler.GetOnChunksDoneCalled());
  EXPECT_EQ(chunk_handler.GetChunkCount(), 3);
}

TEST(ChunkedObliviousHttpGateway, ProvisionKeyAndDecapsulateBufferedRequest) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-05.html#appendix-A
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));

  for (size_t i = 0; i < encapsulated_request_bytes.size(); i++) {
    absl::string_view current_byte(&encapsulated_request_bytes[i], 1);
    QUICHE_EXPECT_OK(instance->DecryptRequest(current_byte, false));
  }

  QUICHE_EXPECT_OK(instance->DecryptRequest("", true));
  EXPECT_TRUE(chunk_handler.GetOnChunksDoneCalled());
  EXPECT_EQ(chunk_handler.GetChunkCount(), 3);
}

TEST(ChunkedObliviousHttpGateway, DecryptingAfterDoneReturnsInvalidArgument) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));

  QUICHE_EXPECT_OK(instance->DecryptRequest(encapsulated_request_bytes, true));

  auto second_decrypt =
      instance->DecryptRequest(encapsulated_request_bytes, true);
  EXPECT_EQ(second_decrypt.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(second_decrypt.message(), "Decrypting is marked as invalid.");
}

TEST(ChunkedObliviousHttpGateway, FinalChunkNotDoneReturnsInvalidArgument) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes("010020", &encapsulated_request_bytes));

  EXPECT_EQ(instance->DecryptRequest(encapsulated_request_bytes, true).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkedObliviousHttpGateway, GettingDecryptErrorSetsGatewayToInvalid) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string invalid_key_request =
      "020020000100014b28f881333e7c164ffc499ad9796f877f4e1051ee6d31bad19dec96c2"
      "08b4726374e469135906992e";
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes(invalid_key_request, &encapsulated_request_bytes));

  EXPECT_EQ(instance->DecryptRequest(encapsulated_request_bytes, false).code(),
            absl::StatusCode::kInvalidArgument);

  auto second_decrypt =
      instance->DecryptRequest(encapsulated_request_bytes, true);
  EXPECT_EQ(second_decrypt.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(second_decrypt.message(), "Decrypting is marked as invalid.");
}

TEST(ChunkedObliviousHttpGateway, InvalidKeyConfigReturnsInvalidArgument) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("990020000100018811eb457e100811c40a0aa71340a1b81d8"
                             "04bb986f736f2f566a7199761a032",
                             &encapsulated_request_bytes));

  EXPECT_EQ(instance->DecryptRequest(encapsulated_request_bytes, false).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkedObliviousHttpGateway, ChunkHandlerOnChunkErrorPropagates) {
  class FailingChunkHandler : public ObliviousHttpChunkHandler {
   public:
    FailingChunkHandler() = default;
    ~FailingChunkHandler() override = default;
    absl::Status OnDecryptedChunk(
        absl::string_view /*decrypted_chunk*/) override {
      return absl::InvalidArgumentError("Invalid data");
    }
    absl::Status OnChunksDone() override {
      return absl::InvalidArgumentError("Invalid data");
    }
  };
  FailingChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));

  EXPECT_EQ(instance->DecryptRequest(encapsulated_request_bytes, true).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkedObliviousHttpGateway, ChunkHandlerOnChunksDoneErrorPropagates) {
  class FailingChunkHandler : public ObliviousHttpChunkHandler {
   public:
    FailingChunkHandler() = default;
    ~FailingChunkHandler() override = default;
    absl::Status OnDecryptedChunk(
        absl::string_view /*decrypted_chunk*/) override {
      return absl::OkStatus();
    }
    absl::Status OnChunksDone() override {
      return absl::InvalidArgumentError("Invalid data");
    }
  };
  FailingChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));

  EXPECT_EQ(instance->DecryptRequest(encapsulated_request_bytes, true).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpGateway, TestDecryptingMultipleRequestsWithSingleInstance) {
  auto instance = ObliviousHttpGateway::Create(
      GetHpkePrivateKey(),
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM));
  // plaintext: "test request 1"
  absl::string_view encrypted_req_1 =
      "010020000100025f20b60306b61ad9ecad389acd752ca75c4e2969469809fe3d84aae137"
      "f73e4ccfe9ba71f12831fdce6c8202fbd38a84c5d8a73ac4c8ea6c10592594845f";
  std::string encrypted_req_1_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(encrypted_req_1, &encrypted_req_1_bytes));
  auto decapsulated_req_1 =
      instance->DecryptObliviousHttpRequest(encrypted_req_1_bytes);
  ASSERT_TRUE(decapsulated_req_1.ok());
  ASSERT_FALSE(decapsulated_req_1->GetPlaintextData().empty());

  // plaintext: "test request 2"
  absl::string_view encrypted_req_2 =
      "01002000010002285ebc2fcad72cc91b378050cac29a62feea9cd97829335ee9fc87e672"
      "4fa13ff2efdff620423d54225d3099088e7b32a5165f805a5d922918865a0a447a";
  std::string encrypted_req_2_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(encrypted_req_2, &encrypted_req_2_bytes));
  auto decapsulated_req_2 =
      instance->DecryptObliviousHttpRequest(encrypted_req_2_bytes);
  ASSERT_TRUE(decapsulated_req_2.ok());
  ASSERT_FALSE(decapsulated_req_2->GetPlaintextData().empty());
}

TEST(ObliviousHttpGateway, TestInvalidHPKEKey) {
  // Invalid private key.
  EXPECT_EQ(ObliviousHttpGateway::Create(
                "Invalid HPKE key",
                GetOhttpKeyConfig(70, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM))
                .status()
                .code(),
            absl::StatusCode::kInternal);
  // Empty private key.
  EXPECT_EQ(ObliviousHttpGateway::Create(
                /*hpke_private_key*/ "",
                GetOhttpKeyConfig(70, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM))
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ChunkedObliviousHttpGateway, TestInvalidHPKEKey) {
  TestChunkHandler chunk_handler;
  // Invalid private key.
  EXPECT_EQ(ChunkedObliviousHttpGateway::Create(
                "Invalid HPKE key",
                GetOhttpKeyConfig(70, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM),
                chunk_handler)
                .status()
                .code(),
            absl::StatusCode::kInternal);
  // Empty private key.
  EXPECT_EQ(ChunkedObliviousHttpGateway::Create(
                /*hpke_private_key*/ "",
                GetOhttpKeyConfig(70, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM),
                chunk_handler)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpGateway, TestObliviousResponseHandling) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(3, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto instance =
      ObliviousHttpGateway::Create(GetHpkePrivateKey(), ohttp_key_config);
  ASSERT_TRUE(instance.ok());
  auto encapsualte_request_on_client =
      ObliviousHttpRequest::CreateClientObliviousRequest(
          "test", GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(encapsualte_request_on_client.ok());
  // Setup Recipient to allow setting up the HPKE context, and subsequently use
  // it to encrypt the response.
  auto decapsulated_req_on_server = instance->DecryptObliviousHttpRequest(
      encapsualte_request_on_client->EncapsulateAndSerialize());
  ASSERT_TRUE(decapsulated_req_on_server.ok());
  auto server_request_context =
      std::move(decapsulated_req_on_server.value()).ReleaseContext();
  auto encapsulate_resp_on_gateway = instance->CreateObliviousHttpResponse(
      "some response", server_request_context);
  ASSERT_TRUE(encapsulate_resp_on_gateway.ok());
  ASSERT_FALSE(encapsulate_resp_on_gateway->EncapsulateAndSerialize().empty());
}

class TestQuicheRandom : public QuicheRandom {
 public:
  TestQuicheRandom(std::string seed) : seed_(seed) {}
  ~TestQuicheRandom() override {}

  void RandBytes(void* data, size_t len) override {
    size_t copy_len = std::min(len, seed_.length());
    memcpy(data, seed_.c_str(), copy_len);
  }

  uint64_t RandUint64() override { return 0; }

  void InsecureRandBytes(void* /*data*/, size_t /*len*/) override {}
  uint64_t InsecureRandUint64() override { return 0; }

 private:
  std::string seed_;
};

TEST(ChunkedObliviousHttpGateway, SingleChunkResponse) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  // Request decryption implicitly sets up the context for response encryption
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));
  QUICHE_EXPECT_OK(instance->DecryptRequest(encapsulated_request_bytes, true));

  // 63 byte response to test final chunk indicator length.
  std::string plaintext_response =
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111";
  absl::StatusOr<std::string> encrypted_response =
      instance->EncryptResponse(plaintext_response, true);
  QUICHE_EXPECT_OK(encrypted_response);
  EXPECT_FALSE(encrypted_response->empty());
  EXPECT_NE(*encrypted_response, plaintext_response);
}

TEST(ChunkedObliviousHttpGateway, MultipleChunkResponse) {
  // Example from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-05.html#appendix-A
  TestChunkHandler chunk_handler;
  std::string response_nonce = "bcce7f4cb921309ba5d62edf1769ef09";
  std::string response_nonce_bytes;
  EXPECT_TRUE(absl::HexStringToBytes(response_nonce, &response_nonce_bytes));
  TestQuicheRandom quiche_random(response_nonce_bytes);
  auto instance =
      CreateChunkedObliviousHttpGateway(chunk_handler, &quiche_random);

  // Request decrypting implicitly sets up the context for response encryption
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));
  QUICHE_EXPECT_OK(instance->DecryptRequest(encapsulated_request_bytes, true));

  std::string plaintext_response = "01";
  std::string plaintext_response_bytes;
  EXPECT_TRUE(
      absl::HexStringToBytes(plaintext_response, &plaintext_response_bytes));
  std::vector<std::string> encrypted_response_chunks;
  absl::StatusOr<std::string> encrypted_response_chunk =
      instance->EncryptResponse(plaintext_response_bytes, false);
  QUICHE_EXPECT_OK(encrypted_response_chunk);
  std::string encrypted_response_chunk_hex =
      absl::BytesToHexString(*encrypted_response_chunk);
  // The first chunk should contain the response nonce.
  EXPECT_EQ(
      encrypted_response_chunk_hex,
      "bcce7f4cb921309ba5d62edf1769ef091179bf1cc87fa0e2c02de4546945aa3d1e48");

  plaintext_response = "40c8";
  EXPECT_TRUE(
      absl::HexStringToBytes(plaintext_response, &plaintext_response_bytes));
  encrypted_response_chunk =
      instance->EncryptResponse(plaintext_response_bytes, false);
  QUICHE_EXPECT_OK(encrypted_response_chunk);
  encrypted_response_chunk_hex =
      absl::BytesToHexString(*encrypted_response_chunk);
  EXPECT_EQ(encrypted_response_chunk_hex,
            "12b348b5bd4c594c16b6170b07b475845d1f32");

  EXPECT_TRUE(
      absl::HexStringToBytes(plaintext_response, &plaintext_response_bytes));
  encrypted_response_chunk =
      instance->EncryptResponse(/*plaintext_payload=*/"", true);
  QUICHE_EXPECT_OK(encrypted_response_chunk);
  encrypted_response_chunk_hex =
      absl::BytesToHexString(*encrypted_response_chunk);
  EXPECT_EQ(encrypted_response_chunk_hex, "00ed9d8a796617a5b27265f4d73247f639");
}

TEST(ChunkedObliviousHttpGateway, EncryptingAfterFinalChunkFails) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  // Request decryption implicitly sets up the context for response encryption
  std::string encapsulated_request_bytes;
  ASSERT_TRUE(absl::HexStringToBytes(kEncapsulatedChunkedRequest,
                                     &encapsulated_request_bytes));
  QUICHE_EXPECT_OK(instance->DecryptRequest(encapsulated_request_bytes, true));

  std::string plaintext_response = "0140c8";
  absl::StatusOr<std::string> encrypted_response =
      instance->EncryptResponse(plaintext_response, true);
  QUICHE_EXPECT_OK(encrypted_response);
  EXPECT_EQ(
      instance->EncryptResponse(plaintext_response, false).status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ChunkedObliviousHttpGateway, EncryptingBeforeDecryptingFails) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string plaintext_response = "0140c8";
  EXPECT_EQ(
      instance->EncryptResponse(plaintext_response, false).status().code(),
      absl::StatusCode::kInternal);
}

TEST(ChunkedObliviousHttpGateway, EncryptionErrorMarksGatewayInvalid) {
  TestChunkHandler chunk_handler;
  auto instance = CreateChunkedObliviousHttpGateway(chunk_handler);

  std::string plaintext_response = "0140c8";
  EXPECT_EQ(
      instance->EncryptResponse(plaintext_response, false).status().code(),
      absl::StatusCode::kInternal);

  EXPECT_EQ(
      instance->EncryptResponse(plaintext_response, false).status().message(),
      "Encrypting is marked as invalid.");
}

TEST(ObliviousHttpGateway,
     TestHandlingMultipleResponsesForMultipleRequestsWithSingleInstance) {
  auto instance = ObliviousHttpGateway::Create(
      GetHpkePrivateKey(),
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM),
      QuicheRandom::GetInstance());
  // Setup contexts first.
  std::string encrypted_request_1_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("010020000100025f20b60306b61ad9ecad389acd752ca75c4"
                             "e2969469809fe3d84aae137"
                             "f73e4ccfe9ba71f12831fdce6c8202fbd38a84c5d8a73ac4c"
                             "8ea6c10592594845f",
                             &encrypted_request_1_bytes));
  auto decrypted_request_1 =
      instance->DecryptObliviousHttpRequest(encrypted_request_1_bytes);
  ASSERT_TRUE(decrypted_request_1.ok());
  std::string encrypted_request_2_bytes;
  ASSERT_TRUE(
      absl::HexStringToBytes("01002000010002285ebc2fcad72cc91b378050cac29a62fee"
                             "a9cd97829335ee9fc87e672"
                             "4fa13ff2efdff620423d54225d3099088e7b32a5165f805a5"
                             "d922918865a0a447a",
                             &encrypted_request_2_bytes));
  auto decrypted_request_2 =
      instance->DecryptObliviousHttpRequest(encrypted_request_2_bytes);
  ASSERT_TRUE(decrypted_request_2.ok());

  // Extract contexts and handle the response for each corresponding request.
  auto oblivious_request_context_1 =
      std::move(decrypted_request_1.value()).ReleaseContext();
  auto encrypted_response_1 = instance->CreateObliviousHttpResponse(
      "test response 1", oblivious_request_context_1);
  ASSERT_TRUE(encrypted_response_1.ok());
  ASSERT_FALSE(encrypted_response_1->EncapsulateAndSerialize().empty());
  auto oblivious_request_context_2 =
      std::move(decrypted_request_2.value()).ReleaseContext();
  auto encrypted_response_2 = instance->CreateObliviousHttpResponse(
      "test response 2", oblivious_request_context_2);
  ASSERT_TRUE(encrypted_response_2.ok());
  ASSERT_FALSE(encrypted_response_2->EncapsulateAndSerialize().empty());
}

TEST(ObliviousHttpGateway, TestWithMultipleThreads) {
  class TestQuicheThread : public QuicheThread {
   public:
    TestQuicheThread(const ObliviousHttpGateway& gateway_receiver,
                     std::string request_payload, std::string response_payload)
        : QuicheThread("gateway_thread"),
          gateway_receiver_(gateway_receiver),
          request_payload_(request_payload),
          response_payload_(response_payload) {}

   protected:
    void Run() override {
      auto decrypted_request =
          gateway_receiver_.DecryptObliviousHttpRequest(request_payload_);
      ASSERT_TRUE(decrypted_request.ok());
      ASSERT_FALSE(decrypted_request->GetPlaintextData().empty());
      auto gateway_request_context =
          std::move(decrypted_request.value()).ReleaseContext();
      auto encrypted_response = gateway_receiver_.CreateObliviousHttpResponse(
          response_payload_, gateway_request_context);
      ASSERT_TRUE(encrypted_response.ok());
      ASSERT_FALSE(encrypted_response->EncapsulateAndSerialize().empty());
    }

   private:
    const ObliviousHttpGateway& gateway_receiver_;
    std::string request_payload_, response_payload_;
  };

  auto gateway_receiver = ObliviousHttpGateway::Create(
      GetHpkePrivateKey(),
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM),
      QuicheRandom::GetInstance());

  std::string request_payload_1;
  ASSERT_TRUE(
      absl::HexStringToBytes("010020000100025f20b60306b61ad9ecad389acd752ca75c4"
                             "e2969469809fe3d84aae137"
                             "f73e4ccfe9ba71f12831fdce6c8202fbd38a84c5d8a73ac4c"
                             "8ea6c10592594845f",
                             &request_payload_1));
  TestQuicheThread t1(*gateway_receiver, request_payload_1, "test response 1");
  std::string request_payload_2;
  ASSERT_TRUE(
      absl::HexStringToBytes("01002000010002285ebc2fcad72cc91b378050cac29a62fee"
                             "a9cd97829335ee9fc87e672"
                             "4fa13ff2efdff620423d54225d3099088e7b32a5165f805a5"
                             "d922918865a0a447a",
                             &request_payload_2));
  TestQuicheThread t2(*gateway_receiver, request_payload_2, "test response 2");
  t1.Start();
  t2.Start();
  t1.Join();
  t2.Join();
}
}  // namespace
}  // namespace quiche
