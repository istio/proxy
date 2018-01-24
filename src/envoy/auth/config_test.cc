/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "config.h"
#include "gtest/gtest.h"

#include "common/json/json_loader.h"

namespace Envoy {
namespace Http {
namespace Auth {

void TestIssuerInfo(const IssuerInfo& a, const IssuerInfo& b) {
  EXPECT_EQ(a.uri, b.uri);
  EXPECT_EQ(a.cluster, b.cluster);
  EXPECT_EQ(a.name, b.name);
  EXPECT_EQ(a.pubkey_type, b.pubkey_type);
  EXPECT_EQ(a.pubkey_cache_expiration_sec, b.pubkey_cache_expiration_sec);
  EXPECT_EQ(a.audiences, b.audiences);
}

TEST(ConfigTest, GoodTwoIssuers) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer1_name",
         "audiences": [
            "audience1",
            "audience2"
          ],
          "pubkey": {
             "type": "jwks",
             "cache_expiration_sec": 100,
             "uri": "issuer1_uri",
             "cluster": "issuer1_cluster"
          }
      },
      {
         "name": "issuer2_name",
         "audiences": [],
          "pubkey": {
             "type": "pem",
             "uri": "issuer2_uri",
             "cluster": "issuer2_cluster"
          }
      }
  ]
}
)";

  IssuerInfo expected_issuer1 = {
      "issuer1_uri",      // std::string uri;
      "issuer1_cluster",  // std::string cluster;
      "issuer1_name",     // std::string name;
      Pubkeys::JWKS,      //  pubkey_type;
      "",                 //  std::string pubkey_value;
      100,                //  int64_t pubkey_cache_expiration_sec;
      {
          // std::set<std::string> audiences;
          "audience1", "audience2",
      },
  };
  IssuerInfo expected_issuer2 = {
      "issuer2_uri",      // std::string uri;
      "issuer2_cluster",  // std::string cluster;
      "issuer2_name",     // std::string name;
      Pubkeys::PEM,       //  pubkey_type;
      "",                 //  std::string pubkey_value;
      600,                //  int64_t pubkey_cache_expiration_sec;
      {},                 // std::set<std::string> audiences;
  };

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  ASSERT_EQ(config.issuers().size(), 2);
  TestIssuerInfo(config.issuers()[0], expected_issuer1);
  TestIssuerInfo(config.issuers()[1], expected_issuer2);
}

TEST(ConfigTest, WrongPubkeyValue) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer_name",
         "audiences": [],
          "pubkey": {
             "type": "pem",
             "value": "invalid-pubkey"
          }
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, EmptyIssuer) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "",
         "audiences": [],
          "pubkey": {
             "type": "jwks",
             "uri": "issuer1_uri",
             "cluster": "issuer1_cluster"
          }
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, WrongAudienceType) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer",
         "audiences": {}
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, MissPubkey) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer",
         "audiences": []
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, WrongPubkeyType) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer",
         "audiences": [],
         "pubkey": {
             "type": "wrong-type"
         }
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, MissingPubkeyUri) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer",
         "audiences": [],
         "pubkey": {
             "type": "jwks",
             "cluster": "issuer1_cluster"
         }
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

TEST(ConfigTest, MissingPubkeyCluster) {
  const char config_json_str[] = R"(
{
   "issuers": [
      {
         "name": "issuer",
         "audiences": [],
         "pubkey": {
             "type": "jwks",
             "uri": "issuer1_uri"
         }
      }
   ]
}
)";

  auto json_obj = Json::Factory::loadFromString(config_json_str);
  Config config(*json_obj);

  auto issuers = config.issuers();
  ASSERT_EQ(issuers.size(), 0);
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
