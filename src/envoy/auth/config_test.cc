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

#include "src/envoy/auth/config.h"
#include "common/json/json_loader.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;

namespace Envoy {
namespace Http {
namespace Auth {

// Config in JSON format
const char kJSONConfig[] = R"(
{
   "jwts": [
      {
         "issuer": "issuer1_name",
         "audiences": [
            "audience1",
            "audience2"
          ],
         "jwks_uri": "http://server1/path1",
         "jwks_uri_envoy_cluster": "issuer1_cluster"
      },
      {
         "issuer": "issuer2_name",
         "audiences": [],
         "jwks_uri": "server2",
         "jwks_uri_envoy_cluster": "issuer2_cluster",
	 "public_key_cache_duration": {
	     "seconds": 600,
	     "nanos": 1000
	 }
      }
  ]
}
)";

const char kProtoText[] = R"(
jwts {
  issuer: "issuer1_name"
  audiences: "audience1"
  audiences: "audience2"
  jwks_uri: "http://server1/path1"
  jwks_uri_envoy_cluster: "issuer1_cluster"
}
jwts {
  issuer: "issuer2_name"
  jwks_uri: "server2"
  public_key_cache_duration {
    seconds: 600
    nanos: 1000
  }
  jwks_uri_envoy_cluster: "issuer2_cluster"
}
)";

TEST(ConfigTest, LoadFromJSON) {
  auto json_obj = Json::Factory::loadFromString(kJSONConfig);
  JwtAuthConfig config(*json_obj);

  std::string out_str;
  TextFormat::PrintToString(config.config(), &out_str);
  GOOGLE_LOG(INFO) << "===" << out_str << "===";

  Config::AuthFilterConfig expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kProtoText, &expected));
  EXPECT_TRUE(MessageDifferencer::Equals(config.config(), expected));
}

TEST(ConfigTest, LoadFromPBStr) {
  Config::AuthFilterConfig expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kProtoText, &expected));

  JwtAuthConfig config(expected.SerializeAsString());
  EXPECT_TRUE(MessageDifferencer::Equals(config.config(), expected));
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
