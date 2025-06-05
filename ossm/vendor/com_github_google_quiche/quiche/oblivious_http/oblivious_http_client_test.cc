#include "quiche/oblivious_http/oblivious_http_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_thread.h"

namespace quiche {

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

const ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                     uint16_t kem_id,
                                                     uint16_t kdf_id,
                                                     uint16_t aead_id) {
  auto ohttp_key_config =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return ohttp_key_config.value();
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

}  // namespace quiche
