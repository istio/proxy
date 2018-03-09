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

#include "src/envoy/http/authn/origin_authenticator.h"
#include "authentication/v1alpha1/policy.pb.h"
#include "common/http/header_map_impl.h"
#include "common/protobuf/protobuf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/authn/test_utils.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

namespace iaapi = istio::authentication::v1alpha1;

using testing::_;
using testing::DoAll;
using testing::MockFunction;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

const char kSingleMethodRule[] = R"(
  binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "abc.xyz"
    }
  }
)";

const char kMultipleMethodsRule[] = R"(
  binding: USE_ORIGIN
  origins {
    jwt {
      issuer: "one"
    }
  }
  origins {
    jwt {
      issuer: "two"
    }
  }
  origins {
    jwt {
      issuer: "three"
    }
  }
)";

const char kPeerBinding[] = R"(
  binding: USE_PEER
  origins {
    jwt {
      issuer: "abc.xyz"
    }
  }
)";

class MockAuthenticator : public OriginAuthenticator {
 public:
  MockAuthenticator(FilterContext* filter_context,
                    const DoneCallback& done_callback,
                    const iaapi::CredentialRule& rule)
      : OriginAuthenticator(filter_context, done_callback, rule) {}

  MOCK_CONST_METHOD2(validateX509,
                     void(const iaapi::MutualTls&, const MethodDoneCallback&));
  MOCK_CONST_METHOD2(validateJwt,
                     void(const iaapi::Jwt&, const MethodDoneCallback&));
};

class OriginAuthenticatorTest : public testing::TestWithParam<bool> {
 public:
  OriginAuthenticatorTest()
      : request_headers_{{":method", "GET"}, {":path", "/"}} {}
  virtual ~OriginAuthenticatorTest() {}

  void SetUp() override {
    filter_context_.setHeaders(&request_headers_);
    expected_result_when_pass_ = TestUtilities::AuthNResultFromString(R"(
      principal: "foo"
      origin {
        user: "foo"
        presenter: "istio.io"
      }
    )");
    set_peer_ = GetParam();
    if (set_peer_) {
      auto peer_result = TestUtilities::CreateX509Payload("bar");
      filter_context_.setPeerResult(&peer_result);
      expected_result_when_pass_.set_peer_user("bar");
    }
    initial_result_ = filter_context_.authenticationResult();
  }

  void init() {
    authenticator_.reset(new StrictMock<MockAuthenticator>(
        &filter_context_, on_done_callback_.AsStdFunction(), rule_));
  }

 protected:
  std::unique_ptr<StrictMock<MockAuthenticator>> authenticator_;
  StrictMock<TestUtilities::MockFilterContext> filter_context_;
  StrictMock<MockFunction<void(bool)>> on_done_callback_;
  Http::TestHeaderMapImpl request_headers_;
  iaapi::CredentialRule rule_;

  // Mock response payload.
  Payload jwt_payload_{TestUtilities::CreateJwtPayload("foo", "istio.io")};
  // Expected result (when authentication pass with mock payload above)
  Result expected_result_when_pass_;
  // Copy of authN result (from filter context) before running authentication.
  // This should be the expected result if authn fail or do nothing.
  Result initial_result_;

  // Indicates peer is set in the authN result before running. This is set from
  // test GetParam()
  bool set_peer_;
};

TEST_P(OriginAuthenticatorTest, Empty) {
  init();
  EXPECT_CALL(on_done_callback_, Call(true)).Times(1);
  authenticator_->run();
  if (set_peer_) {
    initial_result_.set_principal("bar");
  }
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleMethodPass) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kSingleMethodRule, &rule_));

  init();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(testing::InvokeArgument<1>(&jwt_payload_, true));

  EXPECT_CALL(on_done_callback_, Call(true)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, SingleMethodFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kSingleMethodRule, &rule_));

  init();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(testing::InvokeArgument<1>(&jwt_payload_, false));

  EXPECT_CALL(on_done_callback_, Call(false)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, Multiple) {
  ASSERT_TRUE(
      Protobuf::TextFormat::ParseFromString(kMultipleMethodsRule, &rule_));

  init();

  // First method fails, second success (thus third is ignored)
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(2)
      .WillOnce(testing::InvokeArgument<1>(nullptr, false))
      .WillOnce(testing::InvokeArgument<1>(&jwt_payload_, true));

  EXPECT_CALL(on_done_callback_, Call(true)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, MultipleFail) {
  ASSERT_TRUE(
      Protobuf::TextFormat::ParseFromString(kMultipleMethodsRule, &rule_));

  init();

  // All fail.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(3)
      .WillRepeatedly(testing::InvokeArgument<1>(nullptr, false));

  EXPECT_CALL(on_done_callback_, Call(false)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, PeerBindingPass) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kPeerBinding, &rule_));
  // Expected principal is from peer_user.
  expected_result_when_pass_.set_principal(initial_result_.peer_user());

  init();

  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(testing::InvokeArgument<1>(&jwt_payload_, true));

  EXPECT_CALL(on_done_callback_, Call(true)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(expected_result_when_pass_,
                                      filter_context_.authenticationResult()));
}

TEST_P(OriginAuthenticatorTest, PeerBindingFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(kPeerBinding, &rule_));
  init();

  // All fail.
  EXPECT_CALL(*authenticator_, validateJwt(_, _))
      .Times(1)
      .WillOnce(testing::InvokeArgument<1>(&jwt_payload_, false));

  EXPECT_CALL(on_done_callback_, Call(false)).Times(1);
  authenticator_->run();
  EXPECT_TRUE(TestUtility::protoEqual(initial_result_,
                                      filter_context_.authenticationResult()));
}

INSTANTIATE_TEST_CASE_P(OriginAuthenticatorTests, OriginAuthenticatorTest,
                        testing::Bool());

}  // namespace
}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
