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
#include "contrib/endpoints/src/api_manager/config_manager.h"

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

const char kServiceConfig[] = R"(
{
  "name": "endpoints-test.cloudendpointsapis.com",
  "control": {
     "environment": "http://127.0.0.1:808"
  }
})";

const char kServerConfig[] = R"(
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
  }
}
)";

const char kServerConfigWithUserDefinedMetatdataServer[] = R"(
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
  "service_management_config": {
    "url": "http://servicemanagement.user.com",
    "refresh_interval_ms": 1000
  }
}
)";

const char kServerConfigWithServiceName[] = R"(
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
  "service_name": "service_name_from_server_config"
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

const char kGceMetadataWithServiceName[] = R"(
{
  "project": {
    "projectId": "test-project"
  },
  "instance": {
    "attributes":{
      "endpoints-service-name": "service_name_from_master"
    }
  }
}
)";

const char kGceMetadataWithoutServiceNameConfigId[] = R"(
{
  "project": {
    "projectId": "test-project"
  },
  "instance": {
    "attributes":{
    }
  }
}
)";

const char kGceMetadataWithServiceNameAndConfigId[] = R"(
{
  "project": {
    "projectId": "test-project"
  },
  "instance": {
    "attributes":{
      "endpoints-service-name": "service_name_from_metadata",
      "endpoints-service-config-id":"2017-05-01r1"
    }
  }
}
)";

const char kGceMetadataWithServiceNameAndNoConfigId[] = R"(
{
  "project": {
    "projectId": "test-project"
  },
  "instance": {
    "attributes":{
      "endpoints-service-name": "service_name_from_metadata"
    }
  }
}
)";

const char kGceMetadataWithServiceNameAndInvalidConfigId[] = R"(
{
  "project": {
    "projectId": "test-project"
  },
  "instance": {
    "attributes":{
      "endpoints-service-name": "service_name_from_metadata",
      "endpoints-service-config-id":"invalid"
    }
  }
}
)";

const char kMetaData[] = R"(
{
    "instance": {
        "attributes": {
            "endpoints-service-config-id": "2017-05-01r0",
            "endpoints-service-name": "service_name_from_meta_data"
        }
    }
}
)";

const char kServiceConfig1[] = R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "id": "2017-05-01r0"
}
)";

const char kServiceConfig2[] = R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "id": "2017-05-01r1"
}
)";

const char kServiceConfig3[] = R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "id": "2017-05-01r2"
}
)";

const char kRolloutsResponse1[] = R"(
{
  "rollouts": [
    {
      "rolloutId": "2017-05-01r0",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "SUCCESS",
      "trafficPercentStrategy": {
        "percentages": {
          "2017-05-01r0": 100
        }
      },
      "serviceName": "service_name_from_server_config"
    }
  ]
}
)";

const char kRolloutsResponseMultipleConfigs[] = R"(
{
  "rollouts": [
    {
      "rolloutId": "2017-05-01r0",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "SUCCESS",
      "trafficPercentStrategy": {
        "percentages": {
          "2017-05-01r0": 50,
          "2017-05-01r1": 30,
          "2017-05-01r2": 20
        }
      },
      "serviceName": "service_name_from_server_config"
    }
  ]
}
)";

const char kRolloutsResponseFailedRollouts[] = R"(
{
  "rollouts": [
    {
      "rolloutId": "2017-05-01r0",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "FAILED",
      "trafficPercentStrategy": {
        "percentages": {
          "2017-05-01r0": 80,
          "2017-05-01r1": 20
        }
      },
      "serviceName": "service_name_from_server_config"
    }
  ]
}
)";

const char kRolloutsResponseFailedRolloutsNextPage[] = R"(
{
  "rollouts": [
    {
      "rolloutId": "2017-05-01r0",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "FAILED",
      "trafficPercentStrategy": {
        "percentages": {
          "2017-05-01r0": 100
        }
      },
      "serviceName": "service_name_from_server_config"
    }
  ],
  "next_page_token": "next_page_token"
}
)";

const char kRolloutsResponseFailedRolloutsPage2[] = R"(
{
  "rollouts": [
    {
      "rolloutId": "2017-05-01r1",
      "createTime": "2017-05-01T22:40:09.884Z",
      "createdBy": "test_user@google.com",
      "status": "SUCCESS",
      "trafficPercentStrategy": {
        "percentages": {
          "2017-05-01r1": 80,
          "2017-05-01r2": 20
        }
      },
      "serviceName": "service_name_from_server_config"
    }
  ]
}
)";

const char kRolloutsResponseEmpty[] = R"(
{
  "rollouts": [
  ]
}
)";

// service_name, config_id in server config
class ConfigManagerServiceNameConfigIdInServerConfTest
    : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env_.get();

    global_context_ = std::make_shared<context::GlobalContext>(
        std::move(env_), kServerConfigWithServiceNameConfigId);
    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
  }

  std::unique_ptr<MockApiManagerEnvironment> env_;
  MockApiManagerEnvironment* raw_env_;

  std::shared_ptr<context::GlobalContext> global_context_;
};

TEST_F(ConfigManagerServiceNameConfigIdInServerConfTest,
       TestServiceNameAndConfigIdFromServerConfig) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/"
             "?recursive=true",
             kGceMetadataWithServiceNameAndConfigId},
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

  ASSERT_EQ("service_name_from_server_config", global_context_->service_name());
  ASSERT_EQ("2017-05-01r1", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {

        ASSERT_EQ(1, list.size());
        ASSERT_EQ(kServiceConfig1, list[0].first);
        ASSERT_EQ(100, list[0].second);

      }));

  config_manager->Init();
}

TEST_F(ConfigManagerServiceNameConfigIdInServerConfTest,
       TestServiceNameAndConfigIdFromServerConfigFailedToFetch) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/"
             "?recursive=true",
             kGceMetadataWithServiceNameAndConfigId}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("service_name_from_server_config", global_context_->service_name());
  ASSERT_EQ("2017-05-01r1", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ("ABORTED: Failed to load configs", status.ToString());
      }));

  config_manager->Init();
}

// service_name in server_config
class ConfigManagerNameInServerConfTest : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env_.get();

    std::unique_ptr<Config> config = Config::Create(raw_env_, kServiceConfig);
    ASSERT_NE(config.get(), nullptr);

    global_context_ = std::make_shared<context::GlobalContext>(
        std::move(env_), kServerConfigWithServiceName);
    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
  }

  std::unique_ptr<MockApiManagerEnvironment> env_;
  MockApiManagerEnvironment* raw_env_;

  std::shared_ptr<context::GlobalContext> global_context_;
};

TEST_F(ConfigManagerNameInServerConfTest,
       TestNameFromServerConfAndConfigIdFromMetadata) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true",
             kGceMetadataWithServiceNameAndConfigId},
            {"https://servicemanagement.googleapis.com/v1/services/"
             "service_name_from_server_config/configs/2017-05-01r1",
             kServiceConfig2}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("service_name_from_server_config", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [this](const utils::Status& status,
                              std::vector<std::pair<std::string, int>>& list) {

        ASSERT_EQ("OK", status.ToString());
        ASSERT_EQ("2017-05-01r1", global_context_->config_id());

        ASSERT_EQ(1, list.size());
        ASSERT_EQ(kServiceConfig2, list[0].first);
        ASSERT_EQ(100, list[0].second);

      }));

  config_manager->Init();
}

TEST_F(ConfigManagerNameInServerConfTest,
       TestNameFromServerConfAndConfigIdWasNotSpecified) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true",
             kGceMetadataWithServiceNameAndNoConfigId},
            {"https://servicemanagement.googleapis.com/v1/services/"
             "service_name_from_server_config/configs/2017-05-01r1",
             kServiceConfig2}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("service_name_from_server_config", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [this](const utils::Status& status,
                              std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ("ABORTED: API config_id not specified in configuration files",
                  status.ToString());
      }));

  config_manager->Init();
}

// no service_name and config_id in service config
class ConfigManagerMetadataTest : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env_.get();

    std::unique_ptr<Config> config = Config::Create(raw_env_, kServiceConfig);
    ASSERT_NE(config.get(), nullptr);

    global_context_ = std::make_shared<context::GlobalContext>(std::move(env_),
                                                               kServerConfig);

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
  }

  std::unique_ptr<MockApiManagerEnvironment> env_;
  MockApiManagerEnvironment* raw_env_;

  std::shared_ptr<context::GlobalContext> global_context_;
  std::shared_ptr<context::ServiceContext> service_context_;
  std::shared_ptr<context::RequestContext> context_;
};

TEST_F(ConfigManagerMetadataTest, TestServiceNameAndConfigIdFromGceMetadata) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true", kMetaData},
            {"https://servicemanagement.googleapis.com/v1/services/"
             "service_name_from_meta_data/configs/2017-05-01r0",
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

  ASSERT_EQ("", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ("OK", status.ToString());
        ASSERT_EQ(1, list.size());
        ASSERT_EQ(kServiceConfig1, list[0].first);
        ASSERT_EQ(100, list[0].second);
      }));

  config_manager->Init();
}

TEST_F(ConfigManagerMetadataTest, TestNoServiceNameAndConfigIdFromGceMetadata) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true",
             kGceMetadataWithoutServiceNameConfigId}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ(
            "ABORTED: API service name not specified in "
            "configuration files",
            status.ToString());
      }));

  config_manager->Init();
}

TEST_F(ConfigManagerMetadataTest,
       TestServiceNameAndInvalidConfigIdFromGceMetadata) {
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true",
             kGceMetadataWithServiceNameAndInvalidConfigId}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ("ABORTED: Failed to load configs", status.ToString());
      }));

  config_manager->Init();
}

class ConfigManagerUserDefinedMetadataTest : public ::testing::Test {
 public:
  void SetUp() {
    env_.reset(new ::testing::NiceMock<MockApiManagerEnvironment>());
    // save the raw pointer of env before calling std::move(env).
    raw_env_ = env_.get();

    std::unique_ptr<Config> config = Config::Create(raw_env_, kServiceConfig);
    ASSERT_NE(config.get(), nullptr);

    global_context_ = std::make_shared<context::GlobalContext>(
        std::move(env_), kServerConfigWithUserDefinedMetatdataServer);

    std::unique_ptr<MockRequest> request(
        new ::testing::NiceMock<MockRequest>());
  }

  std::unique_ptr<MockApiManagerEnvironment> env_;
  MockApiManagerEnvironment* raw_env_;

  std::shared_ptr<context::GlobalContext> global_context_;
};

TEST_F(ConfigManagerUserDefinedMetadataTest,
       TestServiceNameAndConfigIdFromGceMetadata) {
  // FetchGceMetadata responses with headers and status OK.
  EXPECT_CALL(*raw_env_, DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([](HTTPRequest* req) {
        std::map<std::string, std::string> data = {
            {"http://localhost/computeMetadata/v1/?recursive=true", kMetaData},
            {"http://servicemanagement.user.com/v1/services/"
             "service_name_from_meta_data/configs/2017-05-01r0",
             kServiceConfig1}};

        std::map<std::string, std::string> headers;
        if (data.find(req->url()) == data.end()) {
          std::cout << req->url() << std::endl;
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"),
                          std::move(headers), std::move(data[req->url()]));
        } else {
          req->OnComplete(Status::OK, std::move(headers),
                          std::move(data[req->url()]));
        }
      }));

  ASSERT_EQ("", global_context_->service_name());
  ASSERT_EQ("", global_context_->config_id());

  std::shared_ptr<ConfigManager> config_manager(new ConfigManager(
      global_context_, [](const utils::Status& status,
                          std::vector<std::pair<std::string, int>>& list) {
        ASSERT_EQ("OK", status.ToString());
        ASSERT_EQ(1, list.size());
        ASSERT_EQ(kServiceConfig1, list[0].first);
        ASSERT_EQ(100, list[0].second);
      }));

  config_manager->Init();
}

}  // namespace
}  // namespace service_control_client
}  // namespace google
