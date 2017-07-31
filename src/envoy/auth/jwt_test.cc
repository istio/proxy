#include "jwt.h"

#include "test/test_common/utility.h"

namespace Envoy {
namespace Http {
namespace Auth {

class JwtTest : public testing::Test {
 public:
  const std::string jwt =
      "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0.FxT92eaBr9thDpeWaQh0YFhblVggn86DBpnTa_"
      "DVO4mNoGEkdpuhYq3epHPAs9EluuxdSkDJ3fCoI758ggGDw8GbqyJAcOsH10fBOrQbB7EFRB"
      "CI1xz6-6GEUac5PxyDnwy3liwC_"
      "gK6p4yqOD13EuEY5aoYkeM382tDFiz5Jkh8kKbqKT7h0bhIimniXLDz6iABeNBFouczdPf04"
      "N09hdvlCtAF87Fu1qqfwEQ93A-J7m08bZJoyIPcNmTcYGHwfMR4-lcI5cC_93C_"
      "5BGE1FHPLOHpNghLuM6-rhOtgwZc9ywupn_bBK3QzuAoDnYwpqQhgQL_CdUD_bSHcmWFkw";

  const std::string jwt_header_encoded = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";
  const std::string jwt_payload_encoded =
      "eyJpc3MiOiJodHRwczovL2V4YW1wbGUuY29tIiwic3ViIjoidGVzdEBleGFtcGxlLmNvbSIs"
      "ImV4cCI6MTUwMTI4MTA1OH0";
  const std::string jwt_signature_encoded =
      "FxT92eaBr9thDpeWaQh0YFhblVggn86DBpnTa_"
      "DVO4mNoGEkdpuhYq3epHPAs9EluuxdSkDJ3fCoI758ggGDw8GbqyJAcOsH10fBOrQbB7EFRB"
      "CI1xz6-6GEUac5PxyDnwy3liwC_"
      "gK6p4yqOD13EuEY5aoYkeM382tDFiz5Jkh8kKbqKT7h0bhIimniXLDz6iABeNBFouczdPf04"
      "N09hdvlCtAF87Fu1qqfwEQ93A-J7m08bZJoyIPcNmTcYGHwfMR4-lcI5cC_93C_"
      "5BGE1FHPLOHpNghLuM6-rhOtgwZc9ywupn_bBK3QzuAoDnYwpqQhgQL_CdUD_bSHcmWFkw";

  const std::string header = R"EOF({"alg":"RS256","typ":"JWT"})EOF";
  const std::string payload =
      R"EOF({"iss":"https://example.com","sub":"test@example.com","exp":1501281058})EOF";

  const std::string pubkey =
      "MIIBCgKCAQEAtw7MNxUTxmzWROCD5BqJxmzT7xqc9KsnAjbXCoqEEHDx4WBlfcwk"
      "XHt9e/2+Uwi3Arz3FOMNKwGGlbr7clBY3utsjUs8BTF0kO/poAmSTdSuGeh2mSbc"
      "VHvmQ7X/kichWwx5Qj0Xj4REU3Gixu1gQIr3GATPAIULo5lj/ebOGAa+l0wIG80N"
      "zz1pBtTIUx68xs5ZGe7cIJ7E8n4pMX10eeuh36h+aossePeuHulYmjr4N0/1jG7a"
      "+hHYL6nqwOR3ej0VqCTLS0OloC0LuCpLV7CnSpwbp2Qg/c+MDzQ0TH8g8drIzR5h"
      "Fe9a3NlNRMXgUU5RqbLnR9zfXr7b9oEszQIDAQAB";
};

TEST_F(JwtTest, Util_Split) {
  auto jwt_split = Util::split(jwt, '.');
  EXPECT_EQ(jwt_split.size(), 3);
  auto header = jwt_split[0];
  auto payload = jwt_split[1];
  auto signature = jwt_split[2];
  EXPECT_STREQ(header.c_str(), jwt_header_encoded.c_str());
  EXPECT_STREQ(payload.c_str(), jwt_payload_encoded.c_str());
  EXPECT_STREQ(signature.c_str(), jwt_signature_encoded.c_str());
}

TEST_F(JwtTest, Base64url_decode) {
  auto header_str = Base64url::decode(jwt_header_encoded);
  EXPECT_STREQ(header_str.c_str(), header.c_str());
  auto payload_str = Base64url::decode(jwt_payload_encoded);
  EXPECT_STREQ(payload_str.c_str(), payload.c_str());
}

TEST_F(JwtTest, Jwt_decode) {
  Jwt ob = Jwt();
  auto payload = ob.decode(jwt, pubkey);

  //  bool valid = result.first;
  //  rapidjson::Document* payload = result.second;

  EXPECT_TRUE(payload != nullptr);

  EXPECT_TRUE((*payload)["iss"].IsString());
  std::string iss = (*payload)["iss"].GetString();
  EXPECT_STREQ("https://example.com", iss.c_str());

  EXPECT_TRUE((*payload)["sub"].IsString());
  std::string sub = (*payload)["sub"].GetString();
  EXPECT_STREQ("test@example.com", sub.c_str());

  EXPECT_TRUE((*payload)["exp"].IsInt64());
  int64_t exp = (*payload)["exp"].GetInt64();
  EXPECT_EQ(1501281058LL, exp);
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
