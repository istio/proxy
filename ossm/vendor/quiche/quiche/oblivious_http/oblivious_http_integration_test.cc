#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "quiche/oblivious_http/buffers/oblivious_http_response.h"
#include "quiche/oblivious_http/common/oblivious_http_chunk_handler.h"
#include "quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "quiche/oblivious_http/oblivious_http_gateway.h"

namespace quiche {
namespace {

constexpr absl::string_view kRequestTestPayload = "request test payload";
constexpr absl::string_view kResponseTestPayload = "response test payload";
constexpr absl::string_view kHpkePrivateKeyHex =
    "b77431ecfa8f4cfc30d6e467aafa06944dffe28cb9dd1409e33a3045f5adc8a1";
constexpr absl::string_view kHpkePublicKeyHex =
    "6d21cfe09fbea5122f9ebc2eb2a69fcc4f06408cd54aac934f012e76fcdcef62";

absl::StatusOr<std::string> GetHpkeKey(absl::string_view hpke_key_hex) {
  std::string hpke_key_bytes;
  if (!absl::HexStringToBytes(hpke_key_hex, &hpke_key_bytes)) {
    return absl::InvalidArgumentError("Invalid HPKE key.");
  }
  return hpke_key_bytes;
}

class TestChunkHandler : public ObliviousHttpChunkHandler {
 public:
  absl::Status OnDecryptedChunk(absl::string_view decrypted_chunk) override {
    absl::StrAppend(&concatenated_decrypted_chunks_, decrypted_chunk);
    return absl::OkStatus();
  }
  absl::Status OnChunksDone() override {
    on_chunks_done_called_ = true;
    return absl::OkStatus();
  }
  std::string GetConcatenatedDecryptedChunks() const {
    return concatenated_decrypted_chunks_;
  }
  bool AreChunksDone() const { return on_chunks_done_called_; }

 private:
  bool on_chunks_done_called_ = false;
  std::string concatenated_decrypted_chunks_;
};

TEST(ObliviousHttpIntegrationTest, ClientAndGatewayRequestResponse) {
  absl::StatusOr<ObliviousHttpHeaderKeyConfig> key_config =
      ObliviousHttpHeaderKeyConfig::Create(
          /*key_id=*/1, /*kem_id=*/EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
          /*kdf_id=*/EVP_HPKE_HKDF_SHA256, /*aead_id=*/
          EVP_HPKE_AES_128_GCM);
  QUICHE_ASSERT_OK(key_config);
  absl::StatusOr<std::string> hpke_public_key = GetHpkeKey(kHpkePublicKeyHex);
  QUICHE_ASSERT_OK(hpke_public_key);
  absl::StatusOr<std::string> hpke_private_key = GetHpkeKey(kHpkePrivateKeyHex);
  QUICHE_ASSERT_OK(hpke_private_key);

  absl::StatusOr<ObliviousHttpClient> client =
      ObliviousHttpClient::Create(hpke_public_key.value(), key_config.value());
  QUICHE_ASSERT_OK(client);
  absl::StatusOr<ObliviousHttpGateway> gateway = ObliviousHttpGateway::Create(
      hpke_private_key.value(), key_config.value());
  QUICHE_ASSERT_OK(gateway);

  // Request encrypt/decrypt
  absl::StatusOr<ObliviousHttpRequest> request =
      client->CreateObliviousHttpRequest(std::string(kRequestTestPayload));
  QUICHE_ASSERT_OK(request);
  request =
      gateway->DecryptObliviousHttpRequest(request->EncapsulateAndSerialize());
  QUICHE_ASSERT_OK(request);

  EXPECT_EQ(request->GetPlaintextData(), kRequestTestPayload);

  // Response encrypt/decrypt
  ObliviousHttpRequest::Context request_context =
      std::move(*request).ReleaseContext();
  absl::StatusOr<ObliviousHttpResponse> response =
      gateway->CreateObliviousHttpResponse(std::string(kResponseTestPayload),
                                           request_context);
  QUICHE_ASSERT_OK(response);

  response = client->DecryptObliviousHttpResponse(
      response->EncapsulateAndSerialize(), request_context);
  QUICHE_ASSERT_OK(response);

  EXPECT_EQ(response->GetPlaintextData(), kResponseTestPayload);
}

TEST(ObliviousHttpIntegrationTest,
     ChunkedClientAndChunkedGatewayRequestResponseChunks) {
  absl::StatusOr<ObliviousHttpHeaderKeyConfig> key_config =
      ObliviousHttpHeaderKeyConfig::Create(
          /*key_id=*/1, /*kem_id=*/EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
          /*kdf_id=*/EVP_HPKE_HKDF_SHA256, /*aead_id=*/
          EVP_HPKE_AES_128_GCM);
  QUICHE_ASSERT_OK(key_config);
  absl::StatusOr<std::string> hpke_public_key = GetHpkeKey(kHpkePublicKeyHex);
  QUICHE_ASSERT_OK(hpke_public_key);
  absl::StatusOr<std::string> hpke_private_key = GetHpkeKey(kHpkePrivateKeyHex);
  QUICHE_ASSERT_OK(hpke_private_key);

  TestChunkHandler client_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpClient> chunked_client =
      ChunkedObliviousHttpClient::Create(
          hpke_public_key.value(), key_config.value(), &client_chunk_handler);
  QUICHE_ASSERT_OK(chunked_client);

  TestChunkHandler gateway_chunk_handler;
  absl::StatusOr<ChunkedObliviousHttpGateway> gateway =
      ChunkedObliviousHttpGateway::Create(
          hpke_private_key.value(), key_config.value(), &gateway_chunk_handler);
  QUICHE_ASSERT_OK(gateway);

  // Encrypt request in chunks
  absl::string_view request_part1 =
      kRequestTestPayload.substr(0, kRequestTestPayload.length() / 2);
  absl::string_view request_part2 =
      kRequestTestPayload.substr(kRequestTestPayload.length() / 2);

  absl::StatusOr<std::string> req_chunk1_encrypted =
      chunked_client->EncryptRequestChunk(request_part1,
                                          /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(req_chunk1_encrypted);
  absl::StatusOr<std::string> req_chunk2_encrypted =
      chunked_client->EncryptRequestChunk(request_part2,
                                          /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(req_chunk2_encrypted);
  absl::StatusOr<std::string> req_chunk3_encrypted =
      chunked_client->EncryptRequestChunk(/*plaintext_payload=*/"",
                                          /*is_final_chunk=*/true);
  QUICHE_ASSERT_OK(req_chunk3_encrypted);

  // Decrypt request in chunks
  QUICHE_ASSERT_OK(gateway->DecryptRequest(req_chunk1_encrypted.value(),
                                           /*end_stream=*/false));
  QUICHE_ASSERT_OK(gateway->DecryptRequest(req_chunk2_encrypted.value(),
                                           /*end_stream=*/false));
  QUICHE_ASSERT_OK(gateway->DecryptRequest(req_chunk3_encrypted.value(),
                                           /*end_stream=*/true));
  EXPECT_TRUE(gateway_chunk_handler.AreChunksDone());
  EXPECT_EQ(gateway_chunk_handler.GetConcatenatedDecryptedChunks(),
            kRequestTestPayload);

  // Encrypt response in chunks
  absl::string_view response_part1 =
      kResponseTestPayload.substr(0, kResponseTestPayload.length() / 2);
  absl::string_view response_part2 =
      kResponseTestPayload.substr(kResponseTestPayload.length() / 2);

  absl::StatusOr<std::string> resp_chunk1_encrypted =
      gateway->EncryptResponse(response_part1, /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(resp_chunk1_encrypted);
  absl::StatusOr<std::string> resp_chunk2_encrypted =
      gateway->EncryptResponse(response_part2, /*is_final_chunk=*/false);
  QUICHE_ASSERT_OK(resp_chunk2_encrypted);
  absl::StatusOr<std::string> resp_chunk3_encrypted = gateway->EncryptResponse(
      /*plaintext_payload=*/"", /*is_final_chunk=*/true);
  QUICHE_ASSERT_OK(resp_chunk3_encrypted);

  // Decrypt response in chunks
  QUICHE_ASSERT_OK(
      chunked_client->DecryptResponse(resp_chunk1_encrypted.value(),
                                      /*end_stream=*/false));
  QUICHE_ASSERT_OK(
      chunked_client->DecryptResponse(resp_chunk2_encrypted.value(),
                                      /*end_stream=*/false));
  QUICHE_ASSERT_OK(
      chunked_client->DecryptResponse(resp_chunk3_encrypted.value(),
                                      /*end_stream=*/true));
  EXPECT_TRUE(client_chunk_handler.AreChunksDone());
  EXPECT_EQ(client_chunk_handler.GetConcatenatedDecryptedChunks(),
            kResponseTestPayload);
}

}  // namespace
}  // namespace quiche
