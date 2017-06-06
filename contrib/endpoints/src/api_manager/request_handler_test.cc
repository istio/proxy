// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "contrib/endpoints/src/api_manager/api_manager_impl.h"
#include "contrib/endpoints/src/api_manager/mock_api_manager_environment.h"
#include "gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

namespace {

const char kServerConfigWithServiceNameConfigId[] = R"(
{
  "google_authentication_secret": "{}",
  "metadata_server_config": {
    "enabled": true,
    "url": "http://localhost"
  },
  "service_name": "bookstore.test.appspot.com",
  "config_id": "2017-05-01r0"
}
)";

const char kServiceConfig1[] = R"(
{
  "name": "bookstore.test.appspot.com",
  "title": "Bookstore",
  "http": {
    "rules": [
      {
        "selector": "EchoGetMessage",
        "get": "/echo"
      }
    ]
  },
  "usage": {
    "rules": [
      {
        "selector": "EchoGetMessage",
        "allowUnregisteredCalls": true
      }
    ]
  },
  "control": {
    "environment": "servicecontrol.googleapis.com"
  },
  "id": "2017-05-01r0"
}
)";

const char kServiceConfig2[] = R"(
{
  "name": "different.test.appspot.com",
  "title": "Bookstore",
  "control": {
    "environment": "servicecontrol.googleapis.com"
  },
  "id": "2017-05-01r0"
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

const char kReportResponseSucceeded[] = R"(
service_config_id: "2017-02-08r9"

)";

const char kServiceForStatistics[] =
    "name: \"service-name\"\n"
    "control: {\n"
    "  environment: \"http://127.0.0.1:8081\"\n"
    "}\n";

class RequestMock : public Request {
 public:
  RequestMock(std::unordered_map<std::string, std::string> data)
      : data_(data) {}
  virtual ~RequestMock() {}

  std::string GetRequestHTTPMethod() { return data_["method"]; }
  std::string GetQueryParameters() { return data_["query"]; }
  std::string GetRequestPath() { return data_["path"]; }
  std::string GetUnparsedRequestPath() { return data_["path"]; }
  std::string GetClientIP() { return data_["ip"]; }
  std::string GetClientHost() { return data_["host"]; }
  int64_t GetGrpcRequestBytes() { return 0; }
  int64_t GetGrpcResponseBytes() { return 0; }
  int64_t GetGrpcRequestMessageCounts() { return 0; }
  int64_t GetGrpcResponseMessageCounts() { return 0; }

  bool FindQuery(const std::string &name, std::string *query) {
    if (data_.find("query." + name) == data_.end()) {
      return false;
    }
    *query = data_["query." + name];
    return true;
  }

  bool FindHeader(const std::string &name, std::string *header) {
    if (data_.find("header." + name) == data_.end()) {
      return false;
    }
    *header = data_["header." + name];
    return true;
  }

  ::google::api_manager::protocol::Protocol GetFrontendProtocol() {
    return ::google::api_manager::protocol::Protocol::HTTP;
  }

  ::google::api_manager::protocol::Protocol GetBackendProtocol() {
    return ::google::api_manager::protocol::Protocol::HTTPS;
  }

  void SetAuthToken(const std::string &auth_token) {}

  utils::Status AddHeaderToBackend(const std::string &key,
                                   const std::string &value) {
    return utils::Status::OK;
  }

 private:
  std::unordered_map<std::string, std::string> data_;
};

class ResponseMock : public Response {
 public:
  virtual ~ResponseMock() {}
  utils::Status GetResponseStatus() { return utils::Status::OK; }
  std::size_t GetRequestSize() { return 0; }
  std::size_t GetResponseSize() { return 0; }
  utils::Status GetLatencyInfo(service_control::LatencyInfo *info) {
    return utils::Status::OK;
  }
};

class RequestHandlerTest : public ::testing::Test {
 protected:
  RequestHandlerTest() : callback_run_count_(0) {}
  std::shared_ptr<ApiManager> MakeApiManager(
      std::unique_ptr<ApiManagerEnvInterface> env, const char *service_config);
  std::shared_ptr<ApiManager> MakeApiManager(
      std::unique_ptr<ApiManagerEnvInterface> env, const char *service_config,
      const char *server_config);

  void SetUp() {
    callback_run_count_ = 0;
    call_history_.clear();
  }

 protected:
  std::vector<std::string> call_history_;
  int callback_run_count_;

 private:
  ApiManagerFactory factory_;
};

std::shared_ptr<ApiManager> RequestHandlerTest::MakeApiManager(
    std::unique_ptr<ApiManagerEnvInterface> env, const char *service_config) {
  return factory_.CreateApiManager(std::move(env), service_config, "");
}

std::shared_ptr<ApiManager> RequestHandlerTest::MakeApiManager(
    std::unique_ptr<ApiManagerEnvInterface> env, const char *service_config,
    const char *server_config) {
  return factory_.CreateApiManager(std::move(env), service_config,
                                   server_config);
}

TEST_F(RequestHandlerTest, PendingCheckApiManagerInitSucceeded) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([this](HTTPRequest *req) {
        std::unordered_map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "bookstore.test.appspot.com/configs/2017-05-01r0",
             kServiceConfig1},
            {"http://localhost/computeMetadata/v1/?recursive=true", "{}"}};

        call_history_.push_back(req->url());
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {},
                          std::move(kServiceConfig1));
        } else {
          req->OnComplete(Status::OK, {}, std::move(kServiceConfig1));
        }
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/echo"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  request_handler->Check([this](utils::Status status) {
    callback_run_count_++;
    EXPECT_OK(status);
  });

  EXPECT_EQ(0, callback_run_count_);

  api_manager->Init();

  EXPECT_EQ(1, callback_run_count_);

  EXPECT_EQ("OK", api_manager->ConfigLoadingStatus().ToString());
  EXPECT_TRUE(api_manager->Enabled());
  EXPECT_EQ("2017-05-01r0", api_manager->service("2017-05-01r0").id());

  EXPECT_EQ(2, call_history_.size());
  EXPECT_EQ(
      "https://servicemanagement.googleapis.com/v1/services/"
      "bookstore.test.appspot.com/configs/2017-05-01r0",
      call_history_[0]);
  EXPECT_EQ("http://localhost/computeMetadata/v1/?recursive=true",
            call_history_[1]);
}

TEST_F(RequestHandlerTest, PendingCheckApiManagerInitSucceededBackendFailed) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([this](HTTPRequest *req) {
        std::unordered_map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "bookstore.test.appspot.com/configs/2017-05-01r0",
             kServiceConfig1},
            {"http://localhost/computeMetadata/v1/?recursive=true", "{}"}};

        call_history_.push_back(req->url());
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {},
                          std::move(kServiceConfig1));
        } else {
          req->OnComplete(Status::OK, {}, std::move(kServiceConfig1));
        }
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  request_handler->Check([this](utils::Status status) {
    callback_run_count_++;
    // Backend error
    EXPECT_EQ("NOT_FOUND: Method does not exist.", status.ToString());
  });

  EXPECT_EQ(0, callback_run_count_);

  // Initialize the ApiManager then run pending callback.
  api_manager->Init();

  EXPECT_EQ(1, callback_run_count_);

  // Successfully initialized by ConfigManager
  EXPECT_EQ("OK", api_manager->ConfigLoadingStatus().ToString());
  EXPECT_TRUE(api_manager->Enabled());
  EXPECT_EQ("2017-05-01r0", api_manager->service("2017-05-01r0").id());

  EXPECT_EQ(2, call_history_.size());
  EXPECT_EQ(
      "https://servicemanagement.googleapis.com/v1/services/"
      "bookstore.test.appspot.com/configs/2017-05-01r0",
      call_history_[0]);
  EXPECT_EQ("http://localhost/computeMetadata/v1/?recursive=true",
            call_history_[1]);
}

TEST_F(RequestHandlerTest, PendCheckReportApiManagerInitSucceeded) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .WillRepeatedly(Invoke([this](HTTPRequest *req) {

        std::unordered_map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "bookstore.test.appspot.com/configs/2017-05-01r0",
             kServiceConfig1},
            {"http://localhost/computeMetadata/v1/?recursive=true", "{}"}};

        call_history_.push_back(req->url());
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {},
                          std::move(kServiceConfig1));
        } else {
          req->OnComplete(Status::OK, {}, std::move(kServiceConfig1));
        }
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/echo"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  // Pending request callback will be registered here and
  // will be executed after api_manager->Init();
  request_handler->Check([this](utils::Status status) {
    callback_run_count_++;
    // Initialization was succeeded. Backend error
    EXPECT_EQ(1, callback_run_count_);
    EXPECT_OK(status);
  });

  std::unique_ptr<Response> response(new ResponseMock());

  // Pending request callback will be registered here and
  // will be executed after api_manager->Init();
  request_handler->Report(std::move(response), [this]() {
    callback_run_count_++;
    EXPECT_EQ(2, callback_run_count_);
  });

  EXPECT_EQ(0, callback_run_count_);

  // Initialize the ApiManager then run pending callback.
  api_manager->Init();

  // Check two pending callbacks were executed
  EXPECT_EQ(2, callback_run_count_);

  // Successfully initialized by ConfigManager
  EXPECT_EQ("OK", api_manager->ConfigLoadingStatus().ToString());
  EXPECT_TRUE(api_manager->Enabled());
  EXPECT_EQ("2017-05-01r0", api_manager->service("2017-05-01r0").id());

  EXPECT_EQ(2, call_history_.size());
  EXPECT_EQ(
      "https://servicemanagement.googleapis.com/v1/services/"
      "bookstore.test.appspot.com/configs/2017-05-01r0",
      call_history_[0]);
  EXPECT_EQ("http://localhost/computeMetadata/v1/?recursive=true",
            call_history_[1]);
}

TEST_F(RequestHandlerTest, PendigCheckApiManagerInitSucceededReport) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .Times(3)
      .WillRepeatedly(Invoke([this](HTTPRequest *req) {

        std::unordered_map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "bookstore.test.appspot.com/configs/2017-05-01r0",
             kServiceConfig1},
            {"http://localhost/computeMetadata/v1/?recursive=true", "{}"},
            {"https://servicecontrol.googleapis.com/v1/services/"
             "bookstore.test.appspot.com:report",
             kReportResponseSucceeded}};

        call_history_.push_back(req->url());
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {},
                          std::move(kServiceConfig1));
        } else {
          req->OnComplete(Status::OK, {}, std::move(kServiceConfig1));
        }
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/echo"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  std::unique_ptr<Response> response(new ResponseMock());

  request_handler->Check([this](utils::Status status) {
    callback_run_count_++;
    EXPECT_EQ(1, callback_run_count_);
    EXPECT_OK(status);
  });

  EXPECT_EQ(0, callback_run_count_);

  api_manager->Init();

  EXPECT_EQ(1, callback_run_count_);

  EXPECT_EQ("OK", api_manager->ConfigLoadingStatus().ToString());
  EXPECT_TRUE(api_manager->Enabled());
  EXPECT_EQ("2017-05-01r0", api_manager->service("2017-05-01r0").id());

  // Call Report synchronous
  request_handler->Report(std::move(response), [this]() {
    callback_run_count_++;
    EXPECT_EQ(2, callback_run_count_);
  });

  // Report callback was executed before this line
  EXPECT_EQ(2, callback_run_count_);
}

TEST_F(RequestHandlerTest, PendingReportApiManagerInitSucceeded) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .Times(2)
      .WillRepeatedly(Invoke([this](HTTPRequest *req) {

        std::unordered_map<std::string, std::string> data = {
            {"https://servicemanagement.googleapis.com/v1/services/"
             "bookstore.test.appspot.com/configs/2017-05-01r0",
             kServiceConfig1},
            {"https://servicecontrol.googleapis.com/v1/services/"
             "bookstore.test.appspot.com:report",
             "{\"service_config_id\":\"2017-05-01r0\"}"}};

        call_history_.push_back(req->url());
        if (data.find(req->url()) == data.end()) {
          req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {},
                          std::move(kServiceConfig1));
        } else {
          req->OnComplete(Status::OK, {}, std::move(kServiceConfig1));
        }
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/echo"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  std::unique_ptr<Response> response(new ResponseMock());

  request_handler->Report(std::move(response),
                          [this]() { callback_run_count_++; });

  EXPECT_EQ(0, callback_run_count_);

  api_manager->Init();

  EXPECT_EQ(1, callback_run_count_);

  EXPECT_EQ("OK", api_manager->ConfigLoadingStatus().ToString());
  EXPECT_TRUE(api_manager->Enabled());
  EXPECT_EQ("2017-05-01r0", api_manager->service("2017-05-01r0").id());
}

TEST_F(RequestHandlerTest, PendingCheckApiManagerInitializationFailed) {
  std::unique_ptr<MockApiManagerEnvironment> env(
      new ::testing::NiceMock<MockApiManagerEnvironment>());

  EXPECT_CALL(*(env.get()), DoRunHTTPRequest(_))
      .WillOnce(Invoke([this](HTTPRequest *req) {
        EXPECT_EQ(
            "https://servicemanagement.googleapis.com/v1/services/"
            "bookstore.test.appspot.com/configs/2017-05-01r0",
            req->url());

        req->OnComplete(Status(Code::NOT_FOUND, "Not Found"), {}, "");
      }));

  std::shared_ptr<ApiManagerImpl> api_manager(
      std::dynamic_pointer_cast<ApiManagerImpl>(MakeApiManager(
          std::move(env), "", kServerConfigWithServiceNameConfigId)));

  EXPECT_TRUE(api_manager);

  EXPECT_EQ("UNAVAILABLE: Not initialized yet",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_EQ("bookstore.test.appspot.com", api_manager->service_name());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  std::unordered_map<std::string, std::string> data = {{"method", "GET"},
                                                       {"ip", "127.0.0.1"},
                                                       {"host", "localhost"},
                                                       {"path", "/echo"}};

  std::unique_ptr<RequestHandlerInterface> request_handler =
      api_manager->CreateRequestHandler(
          std::unique_ptr<Request>(new RequestMock(data)));

  request_handler->Check([this](utils::Status status) {
    callback_run_count_++;
    EXPECT_OK(status);
  });
  EXPECT_EQ(0, callback_run_count_);

  api_manager->Init();

  EXPECT_EQ(1, callback_run_count_);

  // Unable to download service_config. Failed to load.
  EXPECT_EQ("ABORTED: Failed to download the service config",
            api_manager->ConfigLoadingStatus().ToString());
  EXPECT_FALSE(api_manager->Enabled());
  EXPECT_EQ("", api_manager->service("2017-05-01r0").id());

  // ApiManager initialization was failed.
  // Report callback will be called right away.
  std::unique_ptr<Response> response(new ResponseMock());
  request_handler->Report(std::move(response),
                          [this]() {
    callback_run_count_++;
    EXPECT_EQ(2, callback_run_count_);
  });

  EXPECT_EQ(2, callback_run_count_);
}

}  // namespace

}  // namespace api_manager
}  // namespace google
