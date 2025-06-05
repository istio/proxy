#include <stdint.h>

#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "openssl/hpke.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/oblivious_http/buffers/oblivious_http_response.h"

namespace quiche {
namespace {

struct ObliviousHttpResponseTestStrings {
  std::string test_case_name;
  uint8_t key_id;
  std::string request_plaintext;
  std::string response_plaintext;
};

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

const ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                     uint16_t kem_id,
                                                     uint16_t kdf_id,
                                                     uint16_t aead_id) {
  auto ohttp_key_config =
      ObliviousHttpHeaderKeyConfig::Create(key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return std::move(ohttp_key_config.value());
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
}  // namespace

using ObliviousHttpParameterizedTest =
    test::QuicheTestWithParam<ObliviousHttpResponseTestStrings>;

TEST_P(ObliviousHttpParameterizedTest, TestEndToEndWithOfflineStrings) {
  // For each test case, verify end to end request-handling and
  // response-handling.
  const ObliviousHttpResponseTestStrings &test = GetParam();

  auto ohttp_key_config =
      GetOhttpKeyConfig(test.key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  // Round-trip request flow.
  auto client_req_encap = ObliviousHttpRequest::CreateClientObliviousRequest(
      test.request_plaintext, GetHpkePublicKey(), ohttp_key_config);
  EXPECT_TRUE(client_req_encap.ok());
  ASSERT_FALSE(client_req_encap->EncapsulateAndSerialize().empty());
  auto server_req_decap = ObliviousHttpRequest::CreateServerObliviousRequest(
      client_req_encap->EncapsulateAndSerialize(),
      *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
      ohttp_key_config);
  EXPECT_TRUE(server_req_decap.ok());
  EXPECT_EQ(server_req_decap->GetPlaintextData(), test.request_plaintext);

  // Round-trip response flow.
  auto server_request_context =
      std::move(server_req_decap.value()).ReleaseContext();
  auto server_resp_encap = ObliviousHttpResponse::CreateServerObliviousResponse(
      test.response_plaintext, server_request_context);
  EXPECT_TRUE(server_resp_encap.ok());
  ASSERT_FALSE(server_resp_encap->EncapsulateAndSerialize().empty());
  auto client_request_context =
      std::move(client_req_encap.value()).ReleaseContext();
  auto client_resp_decap = ObliviousHttpResponse::CreateClientObliviousResponse(
      server_resp_encap->EncapsulateAndSerialize(), client_request_context);
  EXPECT_TRUE(client_resp_decap.ok());
  EXPECT_EQ(client_resp_decap->GetPlaintextData(), test.response_plaintext);
}

INSTANTIATE_TEST_SUITE_P(
    ObliviousHttpParameterizedTests, ObliviousHttpParameterizedTest,
    testing::ValuesIn<ObliviousHttpResponseTestStrings>(
        {{"test_case_1", 4, "test request 1", "test response 1"},
         {"test_case_2", 6, "test request 2", "test response 2"},
         {"test_case_3", 7, "test request 3", "test response 3"},
         {"test_case_4", 2, "test request 4", "test response 4"},
         {"test_case_5", 1, "test request 5", "test response 5"},
         {"test_case_6", 7, "test request 6", "test response 6"},
         {"test_case_7", 3, "test request 7", "test response 7"},
         {"test_case_8", 9, "test request 8", "test response 8"},
         {"test_case_9", 3, "test request 9", "test response 9"},
         {"test_case_10", 4, "test request 10", "test response 10"}}),
    [](const testing::TestParamInfo<ObliviousHttpParameterizedTest::ParamType>
           &info) { return info.param.test_case_name; });

TEST(ObliviousHttpIntegrationTest, TestWithCustomRequestResponseLabels) {
  const std::string kRequestLabel = "test_request_label";
  const std::string kResponseLabel = "test_response_label";

  ObliviousHttpResponseTestStrings test = {"", 4, "test_request_plaintext",
                                           "test_response_plaintext"};

  auto ohttp_key_config =
      GetOhttpKeyConfig(test.key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  // Round-trip request flow.
  auto client_req_encap = ObliviousHttpRequest::CreateClientObliviousRequest(
      test.request_plaintext, GetHpkePublicKey(), ohttp_key_config,
      kRequestLabel);
  EXPECT_TRUE(client_req_encap.ok());
  ASSERT_FALSE(client_req_encap->EncapsulateAndSerialize().empty());
  auto server_req_decap = ObliviousHttpRequest::CreateServerObliviousRequest(
      client_req_encap->EncapsulateAndSerialize(),
      *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
      ohttp_key_config, kRequestLabel);
  EXPECT_TRUE(server_req_decap.ok());
  EXPECT_EQ(server_req_decap->GetPlaintextData(), test.request_plaintext);

  auto failed_server_req_decap =
      ObliviousHttpRequest::CreateServerObliviousRequest(
          client_req_encap->EncapsulateAndSerialize(),
          *(ConstructHpkeKey(GetHpkePrivateKey(), ohttp_key_config)),
          ohttp_key_config);
  EXPECT_FALSE(failed_server_req_decap.ok());

  // Round-trip response flow.
  auto server_request_context =
      std::move(server_req_decap.value()).ReleaseContext();
  auto server_resp_encap = ObliviousHttpResponse::CreateServerObliviousResponse(
      test.response_plaintext, server_request_context, kResponseLabel);
  EXPECT_TRUE(server_resp_encap.ok());
  ASSERT_FALSE(server_resp_encap->EncapsulateAndSerialize().empty());
  auto client_request_context =
      std::move(client_req_encap.value()).ReleaseContext();
  auto client_resp_decap = ObliviousHttpResponse::CreateClientObliviousResponse(
      server_resp_encap->EncapsulateAndSerialize(), client_request_context,
      kResponseLabel);
  EXPECT_TRUE(client_resp_decap.ok());
  EXPECT_EQ(client_resp_decap->GetPlaintextData(), test.response_plaintext);

  auto failed_client_resp_decap =
      ObliviousHttpResponse::CreateClientObliviousResponse(
          server_resp_encap->EncapsulateAndSerialize(), client_request_context);
  EXPECT_FALSE(failed_client_resp_decap.ok());
}

}  // namespace quiche
