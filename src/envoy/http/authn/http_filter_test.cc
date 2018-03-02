#include "authentication/v1alpha1/policy.pb.h"
#include "common/http/header_map_impl.h"
#include "common/protobuf/protobuf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/envoy/http/authn/http_filter_mock.h"
#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace Envoy {
namespace Http {
namespace {

std::unique_ptr<IstioAuthN::AuthenticatePayload> CreateX509Payload(
    const std::string& user) {
  std::unique_ptr<IstioAuthN::AuthenticatePayload> payload(
      new IstioAuthN::AuthenticatePayload);
  payload->mutable_x509()->set_user(user);
  return payload;
}

std::unique_ptr<IstioAuthN::AuthenticatePayload> CreateJwtPayload(
    const std::string& user, const std::string& issuer) {
  std::unique_ptr<IstioAuthN::AuthenticatePayload> payload(
      new IstioAuthN::AuthenticatePayload);
  payload->mutable_jwt()->set_user(user);
  if (!issuer.empty()) {
    payload->mutable_jwt()->set_issuer(issuer);
  }
  return payload;
}

IstioAuthN::Context ContextFromString(const std::string& text) {
  IstioAuthN::Context context;
  EXPECT_TRUE(Protobuf::TextFormat::ParseFromString(text, &context));
  return context;
}

class AuthentiationFilterTest : public testing::Test {
 public:
  AuthentiationFilterTest()
      : request_headers_{{":method", "GET"}, {":path", "/"}} {}
  ~AuthentiationFilterTest() { filter_->onDestroy(); }

 protected:
  void setup_filter() {
    filter_.reset(new StrictMock<MockAuthenticationFilter>(policy_));
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
  }

  std::unique_ptr<StrictMock<MockAuthenticationFilter>> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  Http::TestHeaderMapImpl request_headers_;
  istio::authentication::v1alpha1::Policy policy_;
};

TEST_F(AuthentiationFilterTest, EmptyPolicy) {
  setup_filter();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
}

TEST_F(AuthentiationFilterTest, PeerMTls) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateX509Payload("foo"), IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(
      ContextFromString("principal: \"foo\" peer_user: \"foo\""),
      filter_->context()));
}

TEST_F(AuthentiationFilterTest, PeerMtlsFailed) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(nullptr, IstioAuthN::Status::FAILED);
          })));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::HeaderMap& headers, bool) {
        EXPECT_STREQ("401", headers.Status()->value().c_str());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(
      TestUtility::protoEqual(ContextFromString(""), filter_->context()));
}

TEST_F(AuthentiationFilterTest, PeerJwt) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      jwt {
        issuer: "abc.xyz"
      }
    }
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateJwtPayload("foo", "istio.io"),
                     IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(
      ContextFromString("principal: \"foo\" peer_user: \"foo\""),
      filter_->context()));
}

TEST_F(AuthentiationFilterTest, Origin) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    credential_rules [
      {
        binding: USE_ORIGIN
        origins {
          jwt {
            issuer: "abc.xyz"
          }
        }
      }
    ]
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateJwtPayload("foo", "istio.io"),
                     IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(
      ContextFromString(
          R"EOF(principal: "foo" origin { user: "foo" issuer: "istio.io" })EOF"),
      filter_->context()));
}

TEST_F(AuthentiationFilterTest, OriginWithNoMethod) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    credential_rules {
      binding: USE_ORIGIN
    }
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::HeaderMap& headers, bool) {
        EXPECT_STREQ("401", headers.Status()->value().c_str());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(
      TestUtility::protoEqual(ContextFromString(""), filter_->context()));
}

TEST_F(AuthentiationFilterTest, OriginNoMatchingBindingRule) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    credential_rules {
      binding: USE_ORIGIN
      origins {
        jwt {
          issuer: "abc.xyz"
        }
      }
      matching_peers: "foo"
      matching_peers: "bar"
    }
  )EOF",
                                                    &policy_));

  setup_filter();
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(
      TestUtility::protoEqual(ContextFromString(""), filter_->context()));
}

TEST_F(AuthentiationFilterTest, OriginMatchingBindingRule) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
    credential_rules {
      binding: USE_ORIGIN
      origins {
        jwt {
          issuer: "abc.xyz"
        }
      }
      matching_peers: "foo"
      matching_peers: "bar"
    }
  )EOF",
                                                    &policy_));

  setup_filter();

  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateX509Payload("foo"), IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateJwtPayload("bar", "istio.io"),
                     IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));

  EXPECT_TRUE(TestUtility::protoEqual(ContextFromString(R"EOF(
    principal: "bar"
    peer_user: "foo"
    origin {
      user: "bar"
      issuer: "istio.io"
    })EOF"),
                                      filter_->context()));
}

TEST_F(AuthentiationFilterTest, OriginAughNFail) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
    credential_rules {
      binding: USE_ORIGIN
      origins {
        jwt {
          issuer: "abc.xyz"
        }
      }
    }
  )EOF",
                                                    &policy_));

  setup_filter();

  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateX509Payload("foo"), IstioAuthN::Status::SUCCESS);
          })));
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(nullptr, IstioAuthN::Status::FAILED);
          })));
  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::HeaderMap& headers, bool) {
        EXPECT_STREQ("401", headers.Status()->value().c_str());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(ContextFromString("peer_user: \"foo\""),
                                      filter_->context()));
}

TEST_F(AuthentiationFilterTest, PeerWithExtraJwtPolicy) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
    peers {
      jwt {
        issuer: "istio.io"
      }
    }
    credential_rules {
      binding: USE_PEER
      origins {
        jwt {
          issuer: "foo"
        }
      }
    }
  )EOF",
                                                    &policy_));
  setup_filter();

  // Peer authentication with mTLS/x509 success.
  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateX509Payload("foo"), IstioAuthN::Status::SUCCESS);
          })));

  // Origin authentication also success.
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateJwtPayload("frod", "istio.io"),
                     IstioAuthN::Status::SUCCESS);
          })));

  EXPECT_EQ(Http::FilterHeadersStatus::Continue,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(ContextFromString(R"EOF(
    principal: "foo"
    peer_user: "foo"
    origin {
      user: "frod"
      issuer: "istio.io"
    }
  )EOF"),
                                      filter_->context()));
}

TEST_F(AuthentiationFilterTest, ComplexPolicy) {
  ASSERT_TRUE(Protobuf::TextFormat::ParseFromString(R"EOF(
    peers {
      mtls {
      }
    }
    peers {
      jwt {
        issuer: "istio.io"
      }
    }
    credential_rules {
      binding: USE_ORIGIN
      origins {
        jwt {
          issuer: "foo"
        }
      }
      matching_peers: "foo"
      matching_peers: "bar"
    }
    credential_rules {
      binding: USE_ORIGIN
      origins {
        jwt {
          issuer: "frod"
        }
      }
      origins {
        jwt {
          issuer: "fred"
        }
      }
    }
  )EOF",
                                                    &policy_));
  setup_filter();

  // Peerauthentication with mTLS/x509 fails.
  EXPECT_CALL(*filter_, ValidateX509(_, _, _))
      .Times(1)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(nullptr, IstioAuthN::Status::FAILED);
          })));

  // ValidateJwt is called 3 times:
  // - First for source authentication, which success and set source user to
  // frod.
  // - Then twice for origin authentications (selected for rule matching
  // "frod"), both failed.
  EXPECT_CALL(*filter_, ValidateJwt(_, _, _))
      .Times(3)
      .WillOnce(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(CreateJwtPayload("frod", "istio.io"),
                     IstioAuthN::Status::SUCCESS);
          })))
      .WillRepeatedly(testing::WithArg<2>(
          testing::Invoke([](const AuthenticateDoneCallback& callback) {
            callback(nullptr, IstioAuthN::Status::FAILED);
          })));

  EXPECT_CALL(decoder_callbacks_, encodeHeaders_(_, _))
      .Times(1)
      .WillOnce(testing::Invoke([](Http::HeaderMap& headers, bool) {
        EXPECT_STREQ("401", headers.Status()->value().c_str());
      }));
  EXPECT_EQ(Http::FilterHeadersStatus::StopIteration,
            filter_->decodeHeaders(request_headers_, true));
  EXPECT_TRUE(TestUtility::protoEqual(ContextFromString("peer_user: \"frod\""),
                                      filter_->context()));
}

}  // namespace
}  // namespace Http
}  // namespace Envoy
