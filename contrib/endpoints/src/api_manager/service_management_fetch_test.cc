/* Copyright 2017 Google Inc. All Rights Reserved.
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
#include "contrib/endpoints/src/api_manager/service_management_fetch.h"

#include "contrib/endpoints/src/api_manager/context/global_context.h"
#include "contrib/endpoints/src/api_manager/context/request_context.h"
#include "contrib/endpoints/src/api_manager/context/service_context.h"
#include "contrib/endpoints/src/api_manager/mock_api_manager_environment.h"
#include "contrib/endpoints/src/api_manager/mock_request.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {
namespace {

const char kServiceManagementService[] =
    "https://servicemanagement.googleapis.com";
const char kServiceManagementServiceManager[] =
    "/google.api.servicemanagement.v1.ServiceManager";

const char kServiceConfig[] = R"(
{
  "name": "endpoints-test.cloudendpointsapis.com",
  "control": {
     "environment": "http://127.0.0.1:808"
  }
})";

const char kServiceConfig1[] = R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "id": "2017-05-01r1"
}
)";

const char kServerConfigWithServiceNameConfigId[] = R"(
{
  "google_authentication_secret": "{}",
  "metadata_server_config": {
    "enabled": true,
    "url": "http://localhost"
  },
  "service_control_config": {
    "report_aggregator_config": {
      "cache_entries": 10000,
      "flush_interval_ms": 1000001232
    },
    "quota_aggregator_config": {
      "cache_entries": 300000,
      "refresh_interval_ms": 1000
    }
  },
  "service_name": "service_name_from_server_config",
  "config_id": "2017-05-01r1"
}
)";

}  // namespace

class service_management_fetchTest : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env_.get();

    global_context_ = std::make_shared<context::GlobalContext>(
        std::move(env_), kServerConfigWithServiceNameConfigId);

    if (global_context_->service_account_token()) {
      // register auth token for servicemanagement services
      global_context_->service_account_token()->SetAudience(
          auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES,
          std::string(kServiceManagementService) +
              kServiceManagementServiceManager);
    }

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());

    std::unique_ptr<Config> config = Config::Create(raw_env_, kServiceConfig);
    ASSERT_NE(config.get(), nullptr);

    global_context_->server_config()
        ->mutable_service_management_config()
        ->set_url(kServiceManagementService);
  }

  std::unique_ptr<MockApiManagerEnvironment> env_;
  MockApiManagerEnvironment* raw_env_;

  std::shared_ptr<context::GlobalContext> global_context_;
};

TEST_F(service_management_fetchTest, TestFetchServiceManagementConfig) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "service_name_from_server_config/configs/2017-05-01r1",
             kServiceConfig1}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  FetchServiceManagementConfig(
      global_context_, "2017-05-01r1",
      [](utils::Status status, const std::string& config) {
        ASSERT_EQ(Code::OK, status.code());
        ASSERT_EQ(kServiceConfig1, config);

        std::cout << status.ToString() << std::endl;
      });
}

TEST_F(service_management_fetchTest, TestFetchServiceManagementConfig404) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  FetchServiceManagementConfig(
      global_context_, "2017-05-01r1",
      [](utils::Status status, const std::string& config) {
        ASSERT_EQ(Code::UNAVAILABLE, status.code());
        ASSERT_EQ(
            "UNAVAILABLE: Service management request failed with HTTP response "
            "code 5",
            status.ToString());

        std::cout << status.ToString() << std::endl;
      });
}

}  // namespace api_manager
}  // namespace google
