// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//

#include "contrib/endpoints/src/api_manager/check_security_rules.h"
#include "contrib/endpoints/src/api_manager/context/request_context.h"
#include "contrib/endpoints/src/api_manager/context/service_context.h"
#include "contrib/endpoints/src/api_manager/mock_api_manager_environment.h"
#include "contrib/endpoints/src/api_manager/mock_request.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrCaseEq;
using ::testing::StrEq;
using ::testing::StrNe;

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

namespace {
const char kServiceName[] = R"(
name: "myfirebaseapp.appspot.com")";

const char kBadServiceName[] = R"(
name: "badService.appspot.com")";

const char kProducerProjectId[] = R"(
producer_project_id: "myfirebaseapp"
)";

const char kApis[] = R"(
apis {
  name: "Bookstore"
  version: "v1"
  methods {
    name: "ListShelves"
    request_type_url: "types.googleapis.com/google.protobuf.Empty"
    response_type_url: "types.googleapis.com/Bookstore.ListShelvesResponse"
  }
  methods {
    name: "ListBooks"
    request_type_url: "types.googleapis.com/google.protobuf.Empty"
    response_type_url: "types.googleapis.com/Bookstore.ListBooksResponse"
  }
  methods {
    name: "CreateBook"
    request_type_url: "types.googleapis.com/Bookstore.CreateBookRequest"
    response_type_url: "types.googleapis.com/Bookstore.Book"
  }
})";

const char kAuthentication[] = R"(
authentication {
  providers {
    id: "issuer1"
    issuer: "https://issuer1.com"
  }
  providers {
    id: "issuer2"
    issuer: "https://issuer2.com"
    jwks_uri: "https://issuer2.com/pubkey"
  }
  rules {
    selector: "ListShelves"
    requirements {
      provider_id: "issuer1"
    }
    requirements {
      provider_id: "issuer2"
    }
  }
})";

const char kHttp[] = R"(
http {
  rules {
    selector: "ListShelves"
    get: "/ListShelves"
  }
}
control {
  environment: "http://127.0.0.1:8081"
})";

const char kJwtEmailPayload[] =
    R"({"iss":"https://accounts.google.com","iat":1486575396,"exp":1486578996,"aud":"https://myfirebaseapp.appspot.com","sub":"113424383671131376652","email_verified":true,"azp":"limin-429@appspot.gserviceaccount.com","email":"limin-429@appspot.gserviceaccount.com"})";

static const char kServerConfig[] = R"(
api_check_security_rules_config {
  firebase_server: "https://myfirebaseserver.com"
})";

static const char kRelease[] = R"(
{
  "name": "projects/myfirebaseapp/releases/myfirebaseapp.appspot.com:v1",
  "rulesetName": "projects/myfirebaseapp/rulesets/99045fc0-a5e4-47e2-a665-f88593594b6b",
  "createTime": "2017-01-10T16:52:27.764111Z",
  "updateTime": "2017-01-10T16:52:27.764111Z"
})";

static const char kReleaseError[] = R"(
{
  "error": {
    "code": 404,
    "message": "Requested entity was not found.",
    "status": "NOT_FOUND",
  }
})";

static const char kTestResultFailure[] = R"(
{
  "testResults": [
    {
      "state": "FAILURE"
    }
  ]
}
)";

static const char kTestResultSuccess[] = R"(
{
  "testResults": [
    {
      "state": "SUCCESS"
    }
  ]
}
)";

std::pair<std::string, std::string> GetConfigWithAuthForceDisabled() {
  std::string service_config =
      std::string(kServiceName) + kApis + kAuthentication + kHttp;
  const char server_config[] = R"(
api_authentication_config {
force_disable:  true
}
api_check_security_rules_config {
  firebase_server: "https://myfirebaseserver.com"
}
)";
  return std::make_pair(service_config, server_config);
}

std::pair<std::string, std::string> GetConfigWithNoAuth() {
  std::string service_config = std::string(kServiceName) + kApis + kHttp;
  return std::make_pair(service_config, std::string(kServerConfig));
}

std::pair<std::string, std::string> GetConfigWithoutApis() {
  std::string service_config =
      std::string(kServiceName) + kAuthentication + kHttp;

  return std::make_pair(service_config, std::string(kServerConfig));
}

std::pair<std::string, std::string> GetConfigWithoutServer() {
  std::string service_config =
      std::string(kServiceName) + kApis + kAuthentication + kHttp;
  return std::make_pair(service_config, "");
}

std::pair<std::string, std::string> GetValidConfig() {
  std::string service_config =
      std::string(kServiceName) + kApis + kAuthentication + kHttp;
  return std::make_pair(service_config, kServerConfig);
}

class CheckDisableSecurityRulesTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {
 public:
  void SetUp() {
    std::unique_ptr<MockApiManagerEnvironment> env(
        new ::testing::NiceMock<MockApiManagerEnvironment>());
    raw_env_ = env.get();

    std::string service_config;
    std::string server_config;

    std::tie(service_config, server_config) = GetParam();
    std::unique_ptr<Config> config =
        Config::Create(raw_env_, service_config, server_config);

    ASSERT_TRUE(config != nullptr);

    service_context_ = std::make_shared<context::ServiceContext>(
        std::move(env), std::move(config));

    ASSERT_TRUE(service_context_.get() != nullptr);

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
    raw_request = request.get();

    request_context_ = std::make_shared<context::RequestContext>(
        service_context_, std::move(request));
    EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_)).Times(0);
  }

  MockApiManagerEnvironment *raw_env_;
  MockRequest *raw_request;
  std::shared_ptr<context::RequestContext> request_context_;
  std::shared_ptr<context::ServiceContext> service_context_;
};

TEST_P(CheckDisableSecurityRulesTest, CheckAuthzDisabled) {
  CheckSecurityRules(request_context_,
                     [](Status status) { ASSERT_TRUE(status.ok()); });
}

INSTANTIATE_TEST_CASE_P(ConfigToDisableFirebaseRulesCheck,
                        CheckDisableSecurityRulesTest,
                        testing::Values(GetConfigWithNoAuth(),
                                        GetConfigWithoutApis(),
                                        GetConfigWithoutServer()));

class CheckSecurityRulesTest : public ::testing::Test {
 public:
  void SetUp(std::string service_config, std::string server_config) {
    std::unique_ptr<MockApiManagerEnvironment> env(
        new ::testing::NiceMock<MockApiManagerEnvironment>());
    raw_env_ = env.get();

    std::unique_ptr<Config> config =
        Config::Create(raw_env_, service_config, server_config);
    ASSERT_TRUE(config != nullptr);

    service_context_ = std::make_shared<context::ServiceContext>(
        std::move(env), std::move(config));

    ASSERT_TRUE(service_context_.get() != nullptr);

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
    raw_request_ = request.get();

    ON_CALL(*raw_request_, GetRequestHTTPMethod())
        .WillByDefault(Return(std::string("GET")));

    ON_CALL(*raw_request_, GetRequestPath())
        .WillByDefault(Return(std::string("/ListShelves")));

    request_context_ = std::make_shared<context::RequestContext>(
        service_context_, std::move(request));
    release_url_ =
        "https://myfirebaseserver.com/v1/projects/myfirebaseapp/"
        "releases/myfirebaseapp.appspot.com:v1";

    ruleset_test_url_ =
        "https://myfirebaseserver.com/v1"
        "/projects/myfirebaseapp/rulesets/99045fc0-a5e4-47e2-a665-f88593594b6b"
        ":test?alt=json";
  }

  MockApiManagerEnvironment *raw_env_;
  MockRequest *raw_request_;
  std::shared_ptr<context::RequestContext> request_context_;
  std::shared_ptr<context::ServiceContext> service_context_;
  std::string release_url_;
  std::string ruleset_test_url_;
};

TEST_F(CheckSecurityRulesTest, CheckAuthzFailGetRelease) {
  std::string service_config = std::string(kBadServiceName) +
                               kProducerProjectId + kApis + kAuthentication +
                               kHttp;
  std::string server_config = kServerConfig;
  SetUp(service_config, server_config);

  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(AllOf(
                             Property(&HTTPRequest::url, StrNe(release_url_)),
                             Property(&HTTPRequest::method, StrCaseEq("GET")))))
      .WillOnce(Invoke([](HTTPRequest *req) {

        std::map<std::string, std::string> empty;
        std::string body(kReleaseError);
        req->OnComplete(
            Status(Code::NOT_FOUND, "Requested entity was not found"),
            std::move(empty), std::move(body));

      }));

  EXPECT_CALL(*raw_env_,
              DoRunHTTPRequest(
                  AllOf(Property(&HTTPRequest::url, StrEq(ruleset_test_url_)),
                        Property(&HTTPRequest::method, StrCaseEq("POST")))))
      .Times(0);

  auto ptr = this;
  CheckSecurityRules(request_context_, [ptr](Status status) {
    ptr->raw_env_->LogInfo(status.ToString());
    ASSERT_TRUE(!status.ok());
  });
}

TEST_F(CheckSecurityRulesTest, CheckAuthzFailTestRuleset) {
  std::string service_config = std::string(kServiceName) + kProducerProjectId +
                               kApis + kAuthentication + kHttp;
  std::string server_config = kServerConfig;
  SetUp(service_config, server_config);

  request_context_->set_auth_claims(kJwtEmailPayload);
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(AllOf(
                             Property(&HTTPRequest::url, StrEq(release_url_)),
                             Property(&HTTPRequest::method, StrCaseEq("GET")))))
      .WillOnce(Invoke([](HTTPRequest *req) {

        std::map<std::string, std::string> empty;
        std::string body(kRelease);
        req->OnComplete(Status::OK, std::move(empty), std::move(body));

      }));

  EXPECT_CALL(*raw_env_,
              DoRunHTTPRequest(
                  AllOf(Property(&HTTPRequest::url, StrEq(ruleset_test_url_)),
                        Property(&HTTPRequest::method, StrCaseEq("POST")))))
      .WillOnce(Invoke([](HTTPRequest *req) {
        std::map<std::string, std::string> empty;
        std::string body;
        req->OnComplete(Status(Code::INTERNAL, "Cannot talk to server"),
                        std::move(empty), std::move(body));
      }));

  CheckSecurityRules(request_context_,
                     [](Status status) { ASSERT_FALSE(status.ok()); });
}

TEST_F(CheckSecurityRulesTest, CheckAutzFailWithTestResultFailure) {
  std::string service_config = std::string(kServiceName) + kProducerProjectId +
                               kApis + kAuthentication + kHttp;
  std::string server_config = kServerConfig;
  SetUp(service_config, server_config);

  request_context_->set_auth_claims(kJwtEmailPayload);
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(AllOf(
                             Property(&HTTPRequest::url, StrEq(release_url_)),
                             Property(&HTTPRequest::method, StrCaseEq("GET")))))
      .WillOnce(Invoke([](HTTPRequest *req) {

        std::map<std::string, std::string> empty;
        std::string body(kRelease);
        req->OnComplete(Status::OK, std::move(empty), std::move(body));

      }));

  EXPECT_CALL(*raw_env_,
              DoRunHTTPRequest(
                  AllOf(Property(&HTTPRequest::url, StrEq(ruleset_test_url_)),
                        Property(&HTTPRequest::method, StrCaseEq("POST")))))
      .WillOnce(Invoke([](HTTPRequest *req) {
        std::map<std::string, std::string> empty;
        std::string body = kTestResultFailure;
        req->OnComplete(Status::OK, std::move(empty), std::move(body));
      }));

  CheckSecurityRules(request_context_, [](Status status) {
    ASSERT_TRUE(status.CanonicalCode() == Code::PERMISSION_DENIED);
  });
}

TEST_F(CheckSecurityRulesTest, CheckAuthzSuccess) {
  std::string service_config = std::string(kServiceName) + kProducerProjectId +
                               kApis + kAuthentication + kHttp;
  std::string server_config = kServerConfig;
  SetUp(service_config, server_config);

  request_context_->set_auth_claims(kJwtEmailPayload);
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(AllOf(
                             Property(&HTTPRequest::url, StrEq(release_url_)),
                             Property(&HTTPRequest::method, StrCaseEq("GET")))))
      .WillOnce(Invoke([](HTTPRequest *req) {

        std::map<std::string, std::string> empty;
        std::string body(kRelease);
        req->OnComplete(Status::OK, std::move(empty), std::move(body));

      }));

  EXPECT_CALL(*raw_env_,
              DoRunHTTPRequest(
                  AllOf(Property(&HTTPRequest::url, StrEq(ruleset_test_url_)),
                        Property(&HTTPRequest::method, StrCaseEq("POST")))))
      .WillOnce(Invoke([](HTTPRequest *req) {
        std::map<std::string, std::string> empty;
        std::string body = kTestResultSuccess;
        req->OnComplete(Status::OK, std::move(empty), std::move(body));
      }));

  CheckSecurityRules(request_context_,
                     [](Status status) { ASSERT_TRUE(status.ok()); });
}
}
}  // namespace api_manager
}  // namespace google
