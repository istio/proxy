/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/envoy/utils/token_extractor.h"
#include "gtest/gtest.h"
#include "test/test_common/utility.h"

using ::envoy::config::filter::http::common::v1alpha::JwtVerificationRule;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::_;

namespace Envoy {
namespace Utils {
namespace Jwt {
namespace {

const std::vector<const char*> kExampleRules = {
  R"(
{
  "issuer": "issuer1"
}
)",
  R"(
{
  "issuer": "issuer2",
  "from_headers": [
     {
       "name": "token-header"
     }
  ]
}
)",
  R"(
{
   "issuer": "issuer3",
   "from_params": [
       "token_param"
   ]
}
)",
  R"(
{
   "issuer": "issuer4",
   "from_headers": [
       {
           "name": "token-header"
       }
   ],
   "from_params": [
       "token_param"
   ]
}
)"
};
}  //  namespace

class JwtTokenExtractorTest : public ::testing::Test {
 public:
  void SetUp() { SetupRules(kExampleRules); }

  void SetupRules(const std::vector<const char*> &rule_strs) {
    rules_.clear();
    for(auto rule_str : rule_strs) {
      JwtVerificationRule rule;
      google::protobuf::util::Status status =
        ::google::protobuf::util::JsonStringToMessage(rule_str, &rule);
      ASSERT_TRUE(status.ok());
      rules_.push_back(rule);
    }
    extractor_.reset(new JwtTokenExtractor(rules_));
  }

  std::vector<JwtVerificationRule> rules_;
  std::unique_ptr<JwtTokenExtractor> extractor_;
};

TEST_F(JwtTokenExtractorTest, TestNoToken) {
  auto headers = Http::TestHeaderMapImpl{};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 0);
}

TEST_F(JwtTokenExtractorTest, TestWrongHeaderToken) {
  auto headers = Http::TestHeaderMapImpl{{"wrong-token-header", "jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 0);
}

TEST_F(JwtTokenExtractorTest, TestWrongParamToken) {
  auto headers = Http::TestHeaderMapImpl{{":path", "/path?wrong_token=jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 0);
}

TEST_F(JwtTokenExtractorTest, TestDefaultHeaderLocation) {
  auto headers = Http::TestHeaderMapImpl{{"Authorization", "Bearer jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_EQ(tokens[0]->token(), "jwt_token");
  EXPECT_NE(tokens[0]->header(), nullptr);
  EXPECT_EQ(*tokens[0]->header(), Http::LowerCaseString("Authorization"));

  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer1"));

  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer2"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer3"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer4"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("unknown_issuer"));
}

TEST_F(JwtTokenExtractorTest, TestDefaultParamLocation) {
  auto headers = Http::TestHeaderMapImpl{{":path", "/path?access_token=jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 1);
  EXPECT_EQ(tokens[0]->token(), "jwt_token");
  EXPECT_EQ(tokens[0]->header(), nullptr);

  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer1"));

  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer2"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer3"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer4"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("unknown_issuer"));
}

TEST_F(JwtTokenExtractorTest, TestCustomHeaderToken) {
  auto headers = Http::TestHeaderMapImpl{{"token-header", "jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 1);

  EXPECT_EQ(tokens[0]->token(), "jwt_token");
  EXPECT_NE(tokens[0]->header(), nullptr);
  EXPECT_EQ(*tokens[0]->header(), Http::LowerCaseString("token-header"));

  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer1"));
  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer2"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer3"));
  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer4"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("unknown_issuer"));
}

TEST_F(JwtTokenExtractorTest, TestCustomParamToken) {
  auto headers = Http::TestHeaderMapImpl{{":path", "/path?token_param=jwt_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 1);

  EXPECT_EQ(tokens[0]->token(), "jwt_token");
  EXPECT_EQ(tokens[0]->header(), nullptr);

  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer1"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("issuer2"));
  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer3"));
  EXPECT_TRUE(tokens[0]->IsIssuerAllowed("issuer4"));
  EXPECT_FALSE(tokens[0]->IsIssuerAllowed("unknown_issuer"));
}

TEST_F(JwtTokenExtractorTest, TestMultipleTokens) {
  auto headers = Http::TestHeaderMapImpl{{":path", "/path?token_param=param_token"},
                                   {"token-header", "header_token"}};
  std::vector<std::unique_ptr<JwtTokenExtractor::Token>> tokens;
  extractor_->Extract(headers, &tokens);
  EXPECT_EQ(tokens.size(), 1);

  // Header token first.
  EXPECT_EQ(tokens[0]->token(), "header_token");
  EXPECT_NE(tokens[0]->header(), nullptr);
  EXPECT_EQ(*tokens[0]->header(), Http::LowerCaseString("token-header"));
}

}  // namespace Jwt
}  // namespace Utils
}  // namespace Envoy
