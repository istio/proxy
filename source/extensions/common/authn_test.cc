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

#include "source/extensions/common/authn.h"

#include "source/common/protobuf/protobuf.h"
#include "src/istio/authn/context.pb.h"
#include "src/istio/utils/attribute_names.h"
#include "test/test_common/utility.h"

using istio::authn::Result;

namespace Envoy {
namespace Utils {

class AuthenticationTest : public testing::Test {
protected:
  void SetUp() override {
    test_result_.set_principal("foo");
    test_result_.set_peer_user("bar");
  }
  Result test_result_;
};

TEST_F(AuthenticationTest, SaveAuthAttributesToStruct) {
  istio::authn::Result result;
  ::google::protobuf::Struct data;

  Authentication::SaveAuthAttributesToStruct(result, data);
  EXPECT_TRUE(data.mutable_fields()->empty());

  result.set_principal("principal");
  result.set_peer_user("cluster.local/sa/peeruser/ns/abc/");
  auto origin = result.mutable_origin();
  origin->add_audiences("audiences0");
  origin->add_audiences("audiences1");
  origin->set_presenter("presenter");
  (*origin->mutable_claims()->mutable_fields())["groups"]
      .mutable_list_value()
      ->add_values()
      ->set_string_value("group1");
  (*origin->mutable_claims()->mutable_fields())["groups"]
      .mutable_list_value()
      ->add_values()
      ->set_string_value("group2");
  origin->set_raw_claims("rawclaim");

  Authentication::SaveAuthAttributesToStruct(result, data);
  EXPECT_FALSE(data.mutable_fields()->empty());

  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kRequestAuthPrincipal).string_value(),
            "principal");
  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kSourceUser).string_value(),
            "cluster.local/sa/peeruser/ns/abc/");
  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kSourcePrincipal).string_value(),
            "cluster.local/sa/peeruser/ns/abc/");
  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kSourceNamespace).string_value(), "abc");
  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kRequestAuthAudiences).string_value(),
            "audiences0");
  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kRequestAuthPresenter).string_value(),
            "presenter");

  auto auth_claims = data.fields().at(istio::utils::AttributeName::kRequestAuthClaims);
  EXPECT_EQ(auth_claims.struct_value().fields().at("groups").list_value().values(0).string_value(),
            "group1");
  EXPECT_EQ(auth_claims.struct_value().fields().at("groups").list_value().values(1).string_value(),
            "group2");

  EXPECT_EQ(data.fields().at(istio::utils::AttributeName::kRequestAuthRawClaims).string_value(),
            "rawclaim");
}

} // namespace Utils
} // namespace Envoy
