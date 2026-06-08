#include "quiche/oblivious_http/oblivious_http_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/base.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_thread.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/buffers/oblivious_http_response.h"
#include "quiche/oblivious_http/common/oblivious_http_chunk_handler.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"

namespace quiche {

namespace {

constexpr absl::string_view kPlaintextRequest = "plaintext_request";
constexpr absl::string_view kPlaintextResponse = "plaintext_response";

using ::quiche::test::StatusIs;
using ::testing::ElementsAre;

}  // namespace

// Chunk handler that tests the callback was called in the right order and
// saves the decrypted chunks for later validation.
class TestChunkHandler : public ObliviousHttpChunkHandler {
 public:
  TestChunkHandler() = default;
  ~TestChunkHandler() override = default;
  absl::Status OnDecryptedChunk(absl::string_view decrypted_chunk) override {
    if (on_chunks_done_called_) {
      return absl::FailedPreconditionError(
          "OnDecryptedChunk called after OnChunksDone.");
    }
    if (fail_on_decrypted_chunk_) {
      return absl::OutOfRangeError("Some custom supplied error.");
    }
    decrypted_chunks_.push_back(std::string(decrypted_chunk));
    return absl::OkStatus();
  }

  absl::Status OnChunksDone() override {
    if (on_chunks_done_called_) {
      return absl::FailedPreconditionError(
          "OnChunksDone called more than once.");
    }
    if (fail_on_chunks_done_) {
      return absl::OutOfRangeError("Some custom supplied error.");
    }
    on_chunks_done_called_ = true;
    return absl::OkStatus();
  }

  bool OnChunksDoneCalled() const { return on_chunks_done_called_; }
  const std::vector<std::string>& GetDecryptedChunks() const {
    return decrypted_chunks_;
  }

  void SetFailOnDecryptedChunk(bool fail) { fail_on_decrypted_chunk_ = fail; }

  void SetFailOnChunksDone(bool fail) { fail_on_chunks_done_ = fail; }

 private:
  bool on_chunks_done_called_ = false;
  std::vector<std::string> decrypted_chunks_;
  bool fail_on_decrypted_chunk_ = false;
  bool fail_on_chunks_done_ = false;
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
  return ohttp_key_config.value();
}

absl::StatusOr<ChunkedObliviousHttpClient> CreateChunkedObliviousHttpClient(
    ObliviousHttpChunkHandler& chunk_handler) {
  // Public key from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-06.html#appendix-A-4
  constexpr absl::string_view hpke_public_key =
      "668eb21aace159803974a4c67f08b4152d29bed10735fd08f98ccdd6fe095708";
  std::string hpke_key_bytes;
  if (!absl::HexStringToBytes(hpke_public_key, &hpke_key_bytes)) {
    return absl::InvalidArgumentError("Invalid HPKE public key.");
  }

  return ChunkedObliviousHttpClient::Create(
      hpke_key_bytes,
      GetOhttpKeyConfig(
          /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_128_GCM),
      &chunk_handler);
}

absl::StatusOr<ChunkedObliviousHttpGateway> CreateChunkedObliviousHttpGateway(
    ObliviousHttpChunkHandler* chunk_handler) {
  // Private key from
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-06.html#appendix-A-2
  constexpr absl::string_view kX25519SecretKey =
      "1c190d72acdbe4dbc69e680503bb781a932c70a12c8f3754434c67d8640d8698";
  std::string x25519_secret_key_bytes;
  if (!absl::HexStringToBytes(kX25519SecretKey, &x25519_secret_key_bytes)) {
    return absl::FailedPreconditionError("Invalid X25519 secret key.");
  }

  return ChunkedObliviousHttpGateway::Create(
      x25519_secret_key_bytes,
      GetOhttpKeyConfig(
          /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_128_GCM),
      chunk_handler);
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

TEST(ObliviousHttpClient, TestEncapsulate) {
  auto client = ObliviousHttpClient::Create(
      GetHpkePublicKey(),
      GetOhttpKeyConfig(8, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM));
  ASSERT_TRUE(client.ok());
  auto encrypted_req = client->CreateObliviousHttpRequest("test string 1");
  ASSERT_TRUE(encrypted_req.ok());
  auto serialized_encrypted_req = encrypted_req->EncapsulateAndSerialize();
  ASSERT_FALSE(serialized_encrypted_req.empty());
}

TEST(ObliviousHttpClient, TestEncryptingMultipleRequestsWithSingleInstance) {
  auto client = ObliviousHttpClient::Create(
      GetHpkePublicKey(),
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM));
  ASSERT_TRUE(client.ok());
  auto ohttp_req_1 = client->CreateObliviousHttpRequest("test string 1");
  ASSERT_TRUE(ohttp_req_1.ok());
  auto serialized_ohttp_req_1 = ohttp_req_1->EncapsulateAndSerialize();
  ASSERT_FALSE(serialized_ohttp_req_1.empty());
  auto ohttp_req_2 = client->CreateObliviousHttpRequest("test string 2");
  ASSERT_TRUE(ohttp_req_2.ok());
  auto serialized_ohttp_req_2 = ohttp_req_2->EncapsulateAndSerialize();
  ASSERT_FALSE(serialized_ohttp_req_2.empty());
  EXPECT_NE(serialized_ohttp_req_1, serialized_ohttp_req_2);
}

TEST(ChunkedObliviousHttpClient, DecryptZeroLengthNonFinalResponseChunkFails) {
  TestChunkHandler chunk_handler;
  // Contains the nonce and a zero-length
  // encrypted response chunk.
  std::string nonFinalZeroLengthEncryptedResponseChunk;
  EXPECT_TRUE(absl::HexStringToBytes(
      "a58cfdb3f69b5cb9ae328e25516dfed2109ac5a0b0ce59b9ff5bf6fe1ab2274715",
      &nonFinalZeroLengthEncryptedResponseChunk));

  // The seed used for generating the response context.
  std::string seed;
  EXPECT_TRUE(absl::HexStringToBytes(
      "52c4a758a802cd8b936eceea314432798d5baf2d7e9235dc084ab1b9cfa2f736",
      &seed));

  absl::StatusOr<ObliviousHttpHeaderKeyConfig> key_config =
      ObliviousHttpHeaderKeyConfig::Create(
          /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256, EVP_HPKE_HKDF_SHA256,
          EVP_HPKE_AES_128_GCM);
  QUICHE_ASSERT_OK(key_config);

  absl::StatusOr<ChunkedObliviousHttpClient> chunked_client =
      ChunkedObliviousHttpClient::Create(GetHpkePublicKey(), key_config.value(),
                                         &chunk_handler,

                                         seed);

  QUICHE_ASSERT_OK(chunked_client);
  EXPECT_EQ(
      chunked_client
          ->DecryptResponse(nonFinalZeroLengthEncryptedResponseChunk, false)
          .code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpClient, TestInvalidHPKEKey) {
  // Invalid public key.
  EXPECT_EQ(ObliviousHttpClient::Create(
                "Invalid HPKE key",
                GetOhttpKeyConfig(50, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM))
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
  // Empty public key.
  EXPECT_EQ(ObliviousHttpClient::Create(
                /*hpke_public_key*/ "",
                GetOhttpKeyConfig(50, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM))
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObliviousHttpClient,
     TestTwoSamePlaintextsWillGenerateDifferentEncryptedPayloads) {
  // Due to the nature of the encapsulated_key generated in HPKE being unique
  // for every request, expect different encrypted payloads when encrypting same
  // plaintexts.
  auto client = ObliviousHttpClient::Create(
      GetHpkePublicKey(),
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM));
  ASSERT_TRUE(client.ok());
  auto encrypted_request_1 =
      client->CreateObliviousHttpRequest("same plaintext");
  ASSERT_TRUE(encrypted_request_1.ok());
  auto serialized_encrypted_request_1 =
      encrypted_request_1->EncapsulateAndSerialize();
  ASSERT_FALSE(serialized_encrypted_request_1.empty());
  auto encrypted_request_2 =
      client->CreateObliviousHttpRequest("same plaintext");
  ASSERT_TRUE(encrypted_request_2.ok());
  auto serialized_encrypted_request_2 =
      encrypted_request_2->EncapsulateAndSerialize();
  ASSERT_FALSE(serialized_encrypted_request_2.empty());
  EXPECT_NE(serialized_encrypted_request_1, serialized_encrypted_request_2);
}

TEST(ObliviousHttpClient, TestObliviousResponseHandling) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto encapsulate_req_on_client =
      ObliviousHttpRequest::CreateClientObliviousRequest(
          "test", GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(encapsulate_req_on_client.ok());
  auto decapsulate_req_on_gateway =
      ObliviousHttpRequest::CreateServerObliviousRequest(
          encapsulate_req_on_client->EncapsulateAndSerialize(),
          *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
          ohttp_key_config);
  ASSERT_TRUE(decapsulate_req_on_gateway.ok());
  auto gateway_request_context =
      std::move(decapsulate_req_on_gateway.value()).ReleaseContext();
  auto encapsulate_resp_on_gateway =
      ObliviousHttpResponse::CreateServerObliviousResponse(
          "test response", gateway_request_context);
  ASSERT_TRUE(encapsulate_resp_on_gateway.ok());

  auto client =
      ObliviousHttpClient::Create(GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(client.ok());
  auto client_request_context =
      std::move(encapsulate_req_on_client.value()).ReleaseContext();
  auto decapsulate_resp_on_client = client->DecryptObliviousHttpResponse(
      encapsulate_resp_on_gateway->EncapsulateAndSerialize(),
      client_request_context);
  ASSERT_TRUE(decapsulate_resp_on_client.ok());
  EXPECT_EQ(decapsulate_resp_on_client->GetPlaintextData(), "test response");
}

TEST(ObliviousHttpClient,
     DecryptResponseReceivedByTheClientUsingServersObliviousContext) {
  auto ohttp_key_config =
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto encapsulate_req_on_client =
      ObliviousHttpRequest::CreateClientObliviousRequest(
          "test", GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(encapsulate_req_on_client.ok());
  auto decapsulate_req_on_gateway =
      ObliviousHttpRequest::CreateServerObliviousRequest(
          encapsulate_req_on_client->EncapsulateAndSerialize(),
          *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
          ohttp_key_config);
  ASSERT_TRUE(decapsulate_req_on_gateway.ok());
  auto gateway_request_context =
      std::move(decapsulate_req_on_gateway.value()).ReleaseContext();
  auto encapsulate_resp_on_gateway =
      ObliviousHttpResponse::CreateServerObliviousResponse(
          "test response", gateway_request_context);
  ASSERT_TRUE(encapsulate_resp_on_gateway.ok());

  auto client =
      ObliviousHttpClient::Create(GetHpkePublicKey(), ohttp_key_config);
  ASSERT_TRUE(client.ok());
  auto decapsulate_resp_on_client = client->DecryptObliviousHttpResponse(
      encapsulate_resp_on_gateway->EncapsulateAndSerialize(),
      gateway_request_context);
  ASSERT_TRUE(decapsulate_resp_on_client.ok());
  EXPECT_EQ(decapsulate_resp_on_client->GetPlaintextData(), "test response");
}

TEST(ObliviousHttpClient, TestWithMultipleThreads) {
  class TestQuicheThread : public QuicheThread {
   public:
    TestQuicheThread(const ObliviousHttpClient& client,
                     std::string request_payload,
                     ObliviousHttpHeaderKeyConfig ohttp_key_config)
        : QuicheThread("client_thread"),
          client_(client),
          request_payload_(request_payload),
          ohttp_key_config_(ohttp_key_config) {}

   protected:
    void Run() override {
      auto encrypted_request =
          client_.CreateObliviousHttpRequest(request_payload_);
      ASSERT_TRUE(encrypted_request.ok());
      ASSERT_FALSE(encrypted_request->EncapsulateAndSerialize().empty());
      // Setup recipient and get encrypted response payload.
      auto decapsulate_req_on_gateway =
          ObliviousHttpRequest::CreateServerObliviousRequest(
              encrypted_request->EncapsulateAndSerialize(),
              *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config_)),
              ohttp_key_config_);
      ASSERT_TRUE(decapsulate_req_on_gateway.ok());
      auto gateway_request_context =
          std::move(decapsulate_req_on_gateway.value()).ReleaseContext();
      auto encapsulate_resp_on_gateway =
          ObliviousHttpResponse::CreateServerObliviousResponse(
              "test response", gateway_request_context);
      ASSERT_TRUE(encapsulate_resp_on_gateway.ok());
      ASSERT_FALSE(
          encapsulate_resp_on_gateway->EncapsulateAndSerialize().empty());
      auto client_request_context =
          std::move(encrypted_request.value()).ReleaseContext();
      auto decrypted_response = client_.DecryptObliviousHttpResponse(
          encapsulate_resp_on_gateway->EncapsulateAndSerialize(),
          client_request_context);
      ASSERT_TRUE(decrypted_response.ok());
      ASSERT_FALSE(decrypted_response->GetPlaintextData().empty());
    }

   private:
    const ObliviousHttpClient& client_;
    std::string request_payload_;
    ObliviousHttpHeaderKeyConfig ohttp_key_config_;
  };

  auto ohttp_key_config =
      GetOhttpKeyConfig(1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  auto client =
      ObliviousHttpClient::Create(GetHpkePublicKey(), ohttp_key_config);

  TestQuicheThread t1(*client, "test request 1", ohttp_key_config);
  TestQuicheThread t2(*client, "test request 2", ohttp_key_config);
  t1.Start();
  t2.Start();
  t1.Join();
  t2.Join();
}

TEST(ChunkedObliviousHttpClient, EncryptRequestNonFinalChunkCanNotBeEmpty) {
  TestChunkHandler chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  // TODO(b/425346950): Remove all the redundant checks once ClangTidy gets
  // configured for QUICHE_ASSERT_OK
  if (!chunk_client.ok()) {
    return;
  }
  EXPECT_THAT(chunk_client->EncryptRequestChunk("", /*is_final_chunk=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, EncryptRequestFinalChunkCanBeEmpty) {
  TestChunkHandler chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  absl::StatusOr<std::string> final_chunk =
      chunk_client->EncryptRequestChunk("",
                                        /*is_final_chunk=*/true);
  QUICHE_ASSERT_OK(final_chunk);
  if (!final_chunk.ok()) {
    return;
  }
  // Final chunk uses a non-empty AAD, so encrypting an empty payload results in
  // a non-empty ciphertext.
  // https://www.ietf.org/archive/id/draft-ietf-ohai-chunked-ohttp-06.html#section-6.1-5
  EXPECT_FALSE(final_chunk->empty());

  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }
  QUICHE_ASSERT_OK(chunk_gateway->DecryptRequest(*final_chunk,
                                                 /*end_stream=*/true));
  EXPECT_THAT(gateway_chunk_handler.GetDecryptedChunks(), ElementsAre(""));
}

TEST(ChunkedObliviousHttpClient, EncryptRequestAfterFinalChunkReturnsError) {
  TestChunkHandler chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  absl::StatusOr<std::string> final_chunk =
      chunk_client->EncryptRequestChunk("", /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(final_chunk);
  absl::StatusOr<std::string> next_chunk =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/false);
  EXPECT_THAT(next_chunk.status(),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, EncryptRequestFirstChunkHasHeaderData) {
  TestChunkHandler chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  absl::StatusOr<std::string> first_chunk = chunk_client->EncryptRequestChunk(
      kPlaintextRequest, /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(first_chunk);
  if (!first_chunk.ok()) {
    return;
  }
  absl::StatusOr<std::string> second_chunk = chunk_client->EncryptRequestChunk(
      kPlaintextRequest, /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(second_chunk);
  if (!second_chunk.ok()) {
    return;
  }
  // Encoded header data results in a bigger first chunk.
  EXPECT_GT(first_chunk->size(), second_chunk->size());
}

TEST(ChunkedObliviousHttpClient,
     EncryptRequestMultipleTimesWithSamePlaintextReturnsDifferentCiphertexts) {
  TestChunkHandler chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  absl::StatusOr<std::string> request_with_header =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(request_with_header);
  if (!request_with_header.ok()) {
    return;
  }
  absl::StatusOr<std::string> same_plaintext_chunk_1 =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(same_plaintext_chunk_1);
  if (!same_plaintext_chunk_1.ok()) {
    return;
  }
  absl::StatusOr<std::string> same_plaintext_chunk_2 =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(same_plaintext_chunk_2);
  if (!same_plaintext_chunk_2.ok()) {
    return;
  }

  EXPECT_GT(request_with_header->size(), same_plaintext_chunk_1->size());
  EXPECT_GT(request_with_header->size(), same_plaintext_chunk_2->size());
  EXPECT_NE(*same_plaintext_chunk_1, *same_plaintext_chunk_2);

  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(chunk_gateway->DecryptRequest(*request_with_header,
                                                 /*end_stream=*/false));
  QUICHE_EXPECT_OK(chunk_gateway->DecryptRequest(*same_plaintext_chunk_1,
                                                 /*end_stream=*/false));
  QUICHE_EXPECT_OK(chunk_gateway->DecryptRequest(*same_plaintext_chunk_2,
                                                 /*end_stream=*/false));

  EXPECT_THAT(
      gateway_chunk_handler.GetDecryptedChunks(),
      ElementsAre(kPlaintextRequest, kPlaintextRequest, kPlaintextRequest));
}

TEST(ChunkedObliviousHttpClient,
     SingleChunkEncryptRequestAndDecryptResponseSuccess) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request_chunk =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request_chunk);
  if (!request_chunk.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request_chunk, /*end_stream=*/true));
  EXPECT_THAT(gateway_chunk_handler.GetDecryptedChunks(),
              ElementsAre(kPlaintextRequest));

  absl::StatusOr<std::string> response_chunk =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response_chunk);
  if (!response_chunk.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_client->DecryptResponse(*response_chunk, /*end_stream=*/true));
  EXPECT_THAT(client_chunk_handler.GetDecryptedChunks(),
              ElementsAre(kPlaintextResponse));
  EXPECT_TRUE(client_chunk_handler.OnChunksDoneCalled());
}

TEST(ChunkedObliviousHttpClient,
     MultipleChunksDecryptResponseWhileBufferingSuccess) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }
  std::string plaintext_response_1 = "plaintext_response_1";
  std::string plaintext_response_2 = "plaintext_response_2";
  std::string plaintext_response_3 = "plaintext_response_3";

  absl::StatusOr<std::string> request_chunk =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(request_chunk);
  if (!request_chunk.ok()) {
    return;
  }
  // Initial gateway decrypt is needed to implicitly set up context.
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request_chunk, /*end_stream=*/false));

  absl::StatusOr<std::string> response_chunk_1 =
      chunk_gateway->EncryptResponse(plaintext_response_1,
                                     /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(response_chunk_1);
  if (!response_chunk_1.ok()) {
    return;
  }
  absl::StatusOr<std::string> response_chunk_2 =
      chunk_gateway->EncryptResponse(plaintext_response_2,
                                     /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(response_chunk_2);
  if (!response_chunk_2.ok()) {
    return;
  }
  absl::StatusOr<std::string> response_chunk_3 =
      chunk_gateway->EncryptResponse(plaintext_response_3,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response_chunk_3);
  if (!response_chunk_3.ok()) {
    return;
  }

  std::string full_response =
      absl::StrCat(*response_chunk_1, *response_chunk_2, *response_chunk_3);
  // For each byte, call DecryptResponse with end_stream=false.
  for (size_t i = 0; i < full_response.size() - 1; ++i) {
    QUICHE_EXPECT_OK(chunk_client->DecryptResponse(full_response.substr(i, 1),
                                                   /*end_stream=*/false));
  }
  // The last call to DecryptResponse will have end_stream=true.
  QUICHE_EXPECT_OK(chunk_client->DecryptResponse(
      full_response.substr(full_response.size() - 1, 1),
      /*end_stream=*/true));

  EXPECT_THAT(client_chunk_handler.GetDecryptedChunks(),
              ElementsAre(plaintext_response_1, plaintext_response_2,
                          plaintext_response_3));
  EXPECT_TRUE(client_chunk_handler.OnChunksDoneCalled());
}

TEST(ChunkedObliviousHttpClient, CreateClientWithEmptyPublicKeyFails) {
  TestChunkHandler chunk_handler;
  EXPECT_THAT(ChunkedObliviousHttpClient::Create(
                  /*hpke_public_key=*/"",
                  GetOhttpKeyConfig(
                      /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                      EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM),
                  &chunk_handler),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, CreateClientWithInvalidPublicKeyFails) {
  TestChunkHandler chunk_handler;
  EXPECT_THAT(ChunkedObliviousHttpClient::Create(
                  /*hpke_public_key=*/"invalid_key",
                  GetOhttpKeyConfig(
                      /*key_id=*/1, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                      EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_128_GCM),
                  &chunk_handler),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, DecryptResponseWithCorruptedNonceFails) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response);
  if (!response.ok()) {
    return;
  }
  std::string corrupted_response = *response;
  corrupted_response[0] ^= 0x01;  // Corrupt first byte of nonce.
  EXPECT_THAT(
      chunk_client->DecryptResponse(corrupted_response, /*end_stream=*/true),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, DecryptResponseWithCorruptedChunkFails) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response1 =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/false);
  QUICHE_EXPECT_OK(response1);
  if (!response1.ok()) {
    return;
  }

  std::string corrupted_chunk_response = *response1;
  corrupted_chunk_response[15] ^= 0x01;  // Corrupt byte in chunk data.
                                         // 12 bytes nonce + 1 byte len.
  EXPECT_THAT(chunk_client->DecryptResponse(corrupted_chunk_response,
                                            /*end_stream=*/false),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, DecryptResponseWithCorruptedFinalChunkFails) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response);
  if (!response.ok()) {
    return;
  }
  std::string corrupted_response = *response;
  corrupted_response[15] ^=
      0x01;  // Corrupt byte in chunk data.
             // 12 bytes nonce + 1 byte chunk indicator==0.
  EXPECT_THAT(
      chunk_client->DecryptResponse(corrupted_response, /*end_stream=*/true),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ChunkedObliviousHttpClient, DecryptResponseAfterEndStreamReturnsError) {
  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response);
  if (!response.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_client->DecryptResponse(*response, /*end_stream=*/true));
  EXPECT_THAT(client_chunk_handler.GetDecryptedChunks(),
              ElementsAre(kPlaintextResponse));
  EXPECT_TRUE(client_chunk_handler.OnChunksDoneCalled());

  EXPECT_THAT(chunk_client->DecryptResponse("data", /*end_stream=*/false),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(ChunkedObliviousHttpClient,
     DecryptResponseFailsIfHandlerFailsOnDecryptedChunk) {
  TestChunkHandler client_chunk_handler;
  client_chunk_handler.SetFailOnDecryptedChunk(true);
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response);
  if (!response.ok()) {
    return;
  }
  EXPECT_THAT(chunk_client->DecryptResponse(*response, /*end_stream=*/true),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(ChunkedObliviousHttpClient,
     DecryptResponseFailsIfHandlerFailsOnChunksDone) {
  TestChunkHandler client_chunk_handler;
  client_chunk_handler.SetFailOnChunksDone(true);
  absl::StatusOr<ChunkedObliviousHttpClient> chunk_client =
      CreateChunkedObliviousHttpClient(client_chunk_handler);
  QUICHE_ASSERT_OK(chunk_client);
  if (!chunk_client.ok()) {
    return;
  }
  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> chunk_gateway =
      CreateChunkedObliviousHttpGateway(&gateway_chunk_handler);
  QUICHE_ASSERT_OK(chunk_gateway);
  if (!chunk_gateway.ok()) {
    return;
  }

  absl::StatusOr<std::string> request =
      chunk_client->EncryptRequestChunk(kPlaintextRequest,
                                        /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(request);
  if (!request.ok()) {
    return;
  }
  QUICHE_EXPECT_OK(
      chunk_gateway->DecryptRequest(*request, /*end_stream=*/true));

  absl::StatusOr<std::string> response =
      chunk_gateway->EncryptResponse(kPlaintextResponse,
                                     /*is_final_chunk=*/true);
  QUICHE_EXPECT_OK(response);
  if (!response.ok()) {
    return;
  }
  EXPECT_THAT(chunk_client->DecryptResponse(*response, /*end_stream=*/true),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace quiche
