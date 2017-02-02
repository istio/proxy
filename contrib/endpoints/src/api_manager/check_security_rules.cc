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
#include "contrib/endpoints/src/api_manager/check_security_rules.h"
#include <iostream>
#include <sstream>
#include "contrib/endpoints/src/api_manager/auth/lib/json_util.h"
#include "contrib/endpoints/src/api_manager/proto/security_rules.pb.h"
#include "contrib/endpoints/src/api_manager/utils/marshalling.h"

using ::google::api_manager::auth::GetProperty;
using ::google::api_manager::auth::GetStringValue;
using ::google::api_manager::utils::Status;
using ::google::protobuf::Map;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {
namespace {

const char kFailedFirebaseReleaseFetch[] = "Failed to fetch Firebase Release";
const char kFailedFirebaseTest[] = "Failed to execute Firebase Test";
const char kInvalidResponse[] = "Invalid JSON response from Firebase Service";
const char kTestSuccess[] = "SUCCESS";
const char kHttpGetMethod[] = "GET";
const char kHttpPostMethod[] = "POST";
const char kHttpHeadMethod[] = "HEAD";
const char kHttpOptionsMethod[] = "OPTIONS";
const char kHttpDeleteMethod[] = "DELETE";
const char kFirebaseCreateMethod[] = "create";
const char kFirebaseGetMethod[] = "get";
const char kFirebaseDeleteMethod[] = "delete";
const char kFirebaseUpdateMethod[] = "update";
const char kV1[] = "v1/";
const char kTestQuery[] = ":test?alt=json";
const char kProjects[] = "projects/";
const char kReleases[] = "/releases/";
const char kRulesetName[] = "rulesetName";
const char kTestResults[] = "testResults";
const char kState[] = "state";
const char kToken[] = "token";
const char kAuth[] = "auth";
const char kRequest[] = "request";
const char kContentType[] = "Content-Type";
const char kApplication[] = "application/json";

const std::string &GetFirebaseServer(const context::RequestContext &context) {
  return context.service_context()->config()->GetFirebaseServer();
}

void SetProtoValue(const std::string &key,
                   const ::google::protobuf::Value &value,
                   ::google::protobuf::Value *head) {
  ::google::protobuf::Struct *s = head->mutable_struct_value();
  Map<std::string, google::protobuf::Value> *fields = s->mutable_fields();
  (*fields)[key] = value;
}

const std::string GetReleaseName(const context::RequestContext &context) {
  return context.service_context()->service_name() + ":" +
         context.service_context()->service().apis(0).version();
}

const std::string GetRulesetTestUri(const context::RequestContext &context,
                                    const std::string &ruleset_id) {
  return GetFirebaseServer(context) + kV1 + ruleset_id + kTestQuery;
}

const std::string GetReleaseUrl(const context::RequestContext &context) {
  return GetFirebaseServer(context) + kV1 + kProjects +
         context.service_context()->project_id() + kReleases +
         GetReleaseName(context);
}

const std::string GetOperation(const std::string &httpMethod) {
  if (httpMethod == kHttpPostMethod) {
    return kFirebaseCreateMethod;
  }

  if (httpMethod == kHttpGetMethod || httpMethod == kHttpHeadMethod ||
      httpMethod == kHttpOptionsMethod) {
    return kFirebaseGetMethod;
  }

  if (httpMethod == kHttpDeleteMethod) {
    return kFirebaseDeleteMethod;
  }

  return kFirebaseUpdateMethod;
}

// An AuthzChecker object is created for every incoming request. It does
// authorizaiton by calling Firebase Rules service.
class AuthzChecker : public std::enable_shared_from_this<AuthzChecker> {
 public:
  // Constructor
  AuthzChecker(ApiManagerEnvInterface *env,
               auth::ServiceAccountToken *sa_token);

  // Check for Authorization success or failure
  void Check(std::shared_ptr<context::RequestContext> context,
             std::function<void(Status status)> continuation);

 private:
  // Helper method that invokes the test firebase service api.
  void CallTest(const std::string &ruleset_id,
                std::shared_ptr<context::RequestContext> context,
                std::function<void(Status status)> continuation);

  // Parse the respose for GET RELEASE API call
  Status ParseReleaseResponse(const std::string &json_str,
                              std::string *ruleset_id);

  // Parses the response for the TEST API call
  Status ParseTestResponse(context::RequestContext &context,
                           const std::string &json_str);

  // Builds the request body for the TESP API call.
  Status BuildTestRequestBody(context::RequestContext &context,
                              std::string *result_string);

  // Invoke the HTTP call
  void HttpFetch(const std::string &url, const std::string &method,
                 const std::string &request_body,
                 std::function<void(Status, std::string &&)> continuation);

  // Get the auth token for Firebase service
  const std::string &GetAuthToken() {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_FIREBASE);
  }

  std::shared_ptr<AuthzChecker> GetPtr() { return shared_from_this(); }

  ApiManagerEnvInterface *env_;
  auth::ServiceAccountToken *sa_token_;
};

AuthzChecker::AuthzChecker(ApiManagerEnvInterface *env,
                           auth::ServiceAccountToken *sa_token)
    : env_(env), sa_token_(sa_token) {}

void AuthzChecker::Check(
    std::shared_ptr<context::RequestContext> context,
    std::function<void(Status status)> final_continuation) {
  // TODO: Check service config to see if "useSecurityRules" is specified.
  // If so, call Firebase Rules service TestRuleset API.

  if (!context->service_context()->IsRulesCheckEnabled() ||
      context->method() == nullptr || !context->method()->auth()) {
    env_->LogDebug("Skipping Firebase Rules checks since it is disabled.");
    final_continuation(Status::OK);
    return;
  }

  // Fetch the Release attributes.
  auto checker = GetPtr();
  HttpFetch(GetReleaseUrl(*context), kHttpGetMethod, "",
            [context, final_continuation, checker](Status status,
                                                   std::string &&body) {
              std::string ruleset_id;
              if (status.ok()) {
                checker->env_->LogDebug(
                    std::string("GetReleasName succeeded with ") + body);
                status = checker->ParseReleaseResponse(body, &ruleset_id);
              } else {
                checker->env_->LogError(std::string("GetReleaseName for ") +
                                        GetReleaseUrl(*context.get()) +
                                        " with status " + status.ToString());
                status = Status(Code::INTERNAL, kFailedFirebaseReleaseFetch);
              }

              // If the parsing of the release body is successful, then call the
              // Test Api for firebase rules service.
              if (status.ok()) {
                checker->CallTest(ruleset_id, context, final_continuation);
              } else {
                final_continuation(status);
              }
            });
}

void AuthzChecker::CallTest(const std::string &ruleset_id,
                            std::shared_ptr<context::RequestContext> context,
                            std::function<void(Status status)> continuation) {
  std::string body;
  Status status = BuildTestRequestBody(*context.get(), &body);
  if (!status.ok()) {
    continuation(status);
    return;
  }

  auto checker = GetPtr();
  HttpFetch(GetRulesetTestUri(*context, ruleset_id), kHttpPostMethod, body,
            [context, continuation, checker, ruleset_id](Status status,
                                                         std::string &&body) {

              if (status.ok()) {
                checker->env_->LogDebug(
                    std::string("Test API succeeded with ") + body);
                status = checker->ParseTestResponse(*context.get(), body);
              } else {
                checker->env_->LogError(std::string("Test API failed with ") +
                                        status.ToString());
                status = Status(Code::INTERNAL, kFailedFirebaseTest);
              }

              continuation(status);
            });
}

Status AuthzChecker::ParseReleaseResponse(const std::string &json_str,
                                          std::string *ruleset_id) {
  grpc_json *json = grpc_json_parse_string_with_len(
      const_cast<char *>(json_str.data()), json_str.length());

  if (!json) {
    return Status(Code::INVALID_ARGUMENT, kInvalidResponse);
  }

  Status status = Status::OK;
  const char *id = GetStringValue(json, kRulesetName);
  *ruleset_id = (id == nullptr) ? "" : id;

  if (ruleset_id->empty()) {
    env_->LogError("Empty ruleset Id received from firebase service");
    status = Status(Code::INTERNAL, kInvalidResponse);
  } else {
    env_->LogDebug(std::string("Received ruleset Id: ") + *ruleset_id);
  }

  grpc_json_destroy(json);
  return status;
}

Status AuthzChecker::ParseTestResponse(context::RequestContext &context,
                                       const std::string &json_str) {
  grpc_json *json = grpc_json_parse_string_with_len(
      const_cast<char *>(json_str.data()), json_str.length());

  if (!json) {
    return Status(Code::INVALID_ARGUMENT,
                  "Invalid JSON response from Firebase Service");
  }

  Status status = Status::OK;
  Status invalid = Status(Code::INTERNAL, kInvalidResponse);

  const grpc_json *testResults = GetProperty(json, kTestResults);
  if (testResults == nullptr) {
    env_->LogError("TestResults are null");
    status = invalid;
  } else {
    const char *result = GetStringValue(testResults->child, kState);
    if (result == nullptr) {
      env_->LogInfo("Result state is empty");
      status = invalid;
    } else if (std::string(result) != kTestSuccess) {
      status = Status(Code::PERMISSION_DENIED,
                      std::string("Unauthorized ") +
                          context.request()->GetRequestHTTPMethod() +
                          " access to resource " +
                          context.request()->GetRequestPath(),
                      Status::AUTH);
    }
  }

  grpc_json_destroy(json);
  return status;
}

Status AuthzChecker::BuildTestRequestBody(context::RequestContext &context,
                                          std::string *result_string) {
  proto::TestRulesetRequest request;
  auto *test_case = request.add_test_cases();
  auto httpMethod = context.request()->GetRequestHTTPMethod();

  test_case->set_service_name(context.service_context()->service_name());
  test_case->set_resource_path(context.request()->GetRequestPath());
  test_case->set_operation(GetOperation(httpMethod));
  test_case->set_expectation(proto::TestRulesetRequest::TestCase::ALLOW);

  ::google::protobuf::Value auth;
  ::google::protobuf::Value token;
  ::google::protobuf::Value claims;

  Status status = utils::JsonToProto(context.auth_claims(), &claims);
  if (!status.ok()) {
    env_->LogError(std::string("Error creating Protobuf from claims") +
                   status.ToString());
    return status;
  }

  SetProtoValue(kToken, claims, &token);
  SetProtoValue(kAuth, token, &auth);

  auto *variables = test_case->mutable_variables();
  (*variables)[kRequest] = auth;

  status =
      utils::ProtoToJson(request, result_string, utils::JsonOptions::DEFAULT);
  if (status.ok()) {
    env_->LogDebug(std::string("PRotobuf to JSON string = ") + *result_string);
  } else {
    env_->LogError(std::string("Error creating TestRulesetRequest") +
                   status.ToString());
  }

  return status;
}

void AuthzChecker::HttpFetch(
    const std::string &url, const std::string &method,
    const std::string &request_body,
    std::function<void(Status, std::string &&)> continuation) {
  env_->LogDebug(std::string("Issue HTTP Request to url :") + url +
                 " method : " + method + " body: " + request_body);

  std::unique_ptr<HTTPRequest> request(new HTTPRequest([continuation](
      Status status, std::map<std::string, std::string> &&,
      std::string &&body) { continuation(status, std::move(body)); }));

  if (!request) {
    continuation(Status(Code::INTERNAL, "Out of memory"), "");
    return;
  }

  request->set_method(method).set_url(url).set_auth_token(GetAuthToken());

  if (method != kHttpGetMethod) {
    request->set_header(kContentType, kApplication).set_body(request_body);
  }

  env_->RunHTTPRequest(std::move(request));
}

}  // namespace

void CheckSecurityRules(std::shared_ptr<context::RequestContext> context,
                        std::function<void(Status status)> continuation) {
  std::shared_ptr<AuthzChecker> checker = std::make_shared<AuthzChecker>(
      context->service_context()->env(),
      context->service_context()->service_account_token());
  checker->Check(context, continuation);
}

}  // namespace api_manager
}  // namespace google
