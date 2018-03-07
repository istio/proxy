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

#include "src/envoy/http/authn/filter_context.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/authn/context.pb.h"
#include "src/envoy/http/authn/test_utils.h"
#include "test/test_common/utility.h"

using testing::StrictMock;

namespace iaapi = istio::authentication::v1alpha1;

namespace Envoy {
namespace Http {
namespace {

class FilterContextTest : public testing::Test {
 public:
  FilterContextTest() {}
  ~FilterContextTest() {}

  void SetUp() override {
    filter_context_.reset(
        new StrictMock<AuthNTestUtilities::MockFilterContext>);
  }

  std::unique_ptr<StrictMock<AuthNTestUtilities::MockFilterContext>>
      filter_context_;
};

TEST_F(FilterContextTest, SetPeerResult) {
  filter_context_->setPeerResult(
      AuthNTestUtilities::CreateX509Payload("foo").get());
  EXPECT_TRUE(TestUtility::protoEqual(
      AuthNTestUtilities::AuthNResultFromString("peer_user: \"foo\""),
      filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, SetOriginResult) {
  filter_context_->setOriginResult(
      AuthNTestUtilities::CreateJwtPayload("bar", "istio.io").get());
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        origin {
          user: "bar"
          presenter: "istio.io"
        }
      )"),
                              filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, SetBoth) {
  filter_context_->setPeerResult(
      AuthNTestUtilities::CreateX509Payload("foo").get());
  filter_context_->setOriginResult(
      AuthNTestUtilities::CreateJwtPayload("bar", "istio.io").get());
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        peer_user: "foo"
        origin {
          user: "bar"
          presenter: "istio.io"
        }
      )"),
                              filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, UseOrigin) {
  filter_context_->setPeerResult(
      AuthNTestUtilities::CreateX509Payload("foo").get());
  filter_context_->setOriginResult(
      AuthNTestUtilities::CreateJwtPayload("bar", "istio.io").get());
  filter_context_->setPrincipal(iaapi::CredentialRule::USE_ORIGIN);
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        principal: "bar"
        peer_user: "foo"
        origin {
          user: "bar"
          presenter: "istio.io"
        }
      )"),
                              filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, UseOriginOnEmptyOrigin) {
  filter_context_->setPeerResult(
      AuthNTestUtilities::CreateX509Payload("foo").get());
  filter_context_->setPrincipal(iaapi::CredentialRule::USE_ORIGIN);
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        peer_user: "foo"
      )"),
                              filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, PrincipalUsePeer) {
  filter_context_->setPeerResult(
      AuthNTestUtilities::CreateX509Payload("foo").get());
  filter_context_->setPrincipal(iaapi::CredentialRule::USE_PEER);
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        principal: "foo"
        peer_user: "foo"
      )"),
                              filter_context_->authenticationResult()));
}

TEST_F(FilterContextTest, PrincipalUsePeerOnEmptyPeer) {
  filter_context_->setOriginResult(
      AuthNTestUtilities::CreateJwtPayload("bar", "istio.io").get());
  filter_context_->setPrincipal(iaapi::CredentialRule::USE_PEER);
  EXPECT_TRUE(
      TestUtility::protoEqual(AuthNTestUtilities::AuthNResultFromString(R"(
        origin {
          user: "bar"
          presenter: "istio.io"
        }
      )"),
                              filter_context_->authenticationResult()));
}

}  // namespace
}  // namespace Http
}  // namespace Envoy
