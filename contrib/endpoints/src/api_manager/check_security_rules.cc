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
#include "contrib/endpoints/src/api_manager/auth/lib/json_util.h"
#include <sstream>
#include <iostream>

using ::google::api_manager::auth::GetProperty;
using ::google::api_manager::auth::GetStringValue;
using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

const char kFirebaseServerStaging[] =
    "https://staging-firebaserules.sandbox.googleapis.com/";

const char kFirebaseService[] =
    "/google.firebase.rules.v1.FirebaseRulesService";

const char kFailedFirebaseReleaseFetch[] = "Failed to fetch Firebase Release";
const char kFailedFirebaseTest[] = "Failed to execute Firebase Test";
const char kInvalidResponse[] = "Invalid JSON response from Firebase Service";

AuthzChecker::AuthzChecker(ApiManagerEnvInterface *env,
                           auth::ServiceAccountToken *sa_token)
    : env_(env),
      sa_token_(sa_token) {}

void AuthzChecker::Check(
    std::shared_ptr<context::RequestContext> context,
    std::function<void(Status status)> final_continuation) {
  // TODO: Check service config to see if "useSecurityRules" is specified.
  // If so, call Firebase Rules service TestRuleset API.

  if (!context->service_context()->RequireAuth() ||
      context->method() == nullptr || !context->method()->auth()) {
    env_->LogDebug(
        std::string("Autherization and JWT validation was not performed")
        + " skipping authorization");
    final_continuation(Status::OK);
    return;
  }

  // Fetch the Release attributes.
  auto pChecker = GetPtr();
  HttpFetch(GetReleaseUrl(context), std::string("GET"), std::string(""),
            [context, final_continuation, pChecker] (Status status,
                                           std::string &&body) {
              std::string ruleset_id;
              if (status.ok()) {
                pChecker->env_->LogDebug(
                    std::string("GetReleasName succeeded with ") + body);
                std::tie(status, ruleset_id) = pChecker->ParseReleaseResponse(&body);
              } else {
                pChecker->env_->LogError(std::string("GetReleaseName for ")
                                         + pChecker->GetReleaseUrl(context)
                                         + " with status " + status.ToString());
                status = Status(Code::INTERNAL, kFailedFirebaseReleaseFetch);
              }

              // If the parsing of the release body is successful, then call the
              // Test Api for firebase rules service.
              if (status.ok()) {
                pChecker->CheckAuth(ruleset_id, context, final_continuation);
              } else {
                final_continuation(status);
              }
            });
}

void AuthzChecker::CheckAuth(
    std::string ruleset_id,
    std::shared_ptr<context::RequestContext> context,
    std::function<void(Status status)> continuation) {
  auto pChecker = GetPtr();

  HttpFetch(std::string(kFirebaseServerStaging) + "v1/" + ruleset_id +
            ":test?alt=json",
            std::string("POST"), BuildTestRequestBody(context),
            [context, continuation, pChecker, ruleset_id]
            (Status status, std::string &&body) {

              if (status.ok()) {
                pChecker->env_->LogDebug(
                    std::string("Test API succeeded with ") + body);
                status = pChecker->ParseTestResponse(context, &body);
              } else {
                pChecker->env_->LogError(std::string("Test API failed with ")
                    + status.ToString());
                status = Status(Code::INTERNAL, kFailedFirebaseTest);
              }

              continuation(status);
            });
}

std::pair<Status, std::string> AuthzChecker::ParseReleaseResponse(
    std::string *json_str) {
  grpc_json *json = grpc_json_parse_string_with_len(
      const_cast<char *>(json_str->data()), json_str->length());

  if (!json) {
    return std::make_pair(Status(Code::INVALID_ARGUMENT, kInvalidResponse),
                            "");
  }

  const char *ruleset_id = GetStringValue(json, "rulesetName");
  std::pair<Status, std::string> result = std::make_pair(Status::OK,
                                                         ruleset_id);
  if (ruleset_id == nullptr) {
    env_->LogError("Empty ruleset Id received from firebase service");
    result =std::make_pair(Status(Code::INTERNAL, kInvalidResponse), "");
  } else {
    env_->LogInfo(std::string("Received ruleset Id: ") + ruleset_id);
  }

  grpc_json_destroy(json);
  return result;
}

Status AuthzChecker::ParseTestResponse(
    std::shared_ptr<context::RequestContext> context,
    std::string *json_str) {
  grpc_json *json = grpc_json_parse_string_with_len(
      const_cast<char *>(json_str->data()), json_str->length());


  if (!json) {
    return Status(Code::INVALID_ARGUMENT,
                  "Invalid JSON response from Firebase Service");
  }

  Status status = Status::OK;
  Status invalid = Status(Code::INTERNAL, kInvalidResponse);

  const grpc_json *testResults = GetProperty(json, "testResults");
  if (testResults == nullptr) {
    env_->LogError("TestResults are null");
    status = invalid;
  } else {
    const char *result = GetStringValue(testResults->child, "state");
    if (result == nullptr) {
      env_->LogInfo("Result state is empty");
      status = invalid;
    } else if (std::string(result) != "SUCCESS") {
      status = Status(Code::PERMISSION_DENIED,
              std::string("Unauthorized ")
              + context->request()->GetRequestHTTPMethod()
              + " access to resource " + context->request()->GetRequestPath(),
              Status::AUTH);
    }
  }

  grpc_json_destroy(json);
  return status;
}

std::string AuthzChecker::GetOperation(std::string httpMethod) {
  if (httpMethod == "POST") {
    return std::string("create");
  }

  if (httpMethod == "GET" || httpMethod == "HEAD" || httpMethod == "OPTIONS") {
    return std::string("get");
  }

  if (httpMethod == "DELETE") {
    return std::string("delete");
  }

  return std::string("update");
}

std::string AuthzChecker::BuildTestRequestBody(
    std::shared_ptr<context::RequestContext> context) {

  std::shared_ptr<std::ostringstream> ss(new std::ostringstream);

  int openCount = 0;
  *(ss.get()) << "{"
     <<   Stringify("test_cases") + ": "
     <<     "[ {";
  ++openCount;
  AddToBody("service_name", context->service_context()->service_name(), false, ss);
  AddToBody("resource_path", context->request()->GetRequestPath(),
            false, ss);
  AddToBody("operation", GetOperation(context->request()->GetRequestHTTPMethod()),
            false, ss);
  AddToBody("expectation", "ALLOW", false, ss);
  AddToBody("variables", ss);
  ++openCount;
  AddToBody("request", ss);
  ++openCount;
  AddToBody("auth", ss);
  ++openCount;

  *(ss.get()) << Stringify("token") + ": " << context->auth_claims();

  while(openCount-- > 0) {
    *(ss.get()) << "} ";
  }

  *(ss.get()) << "] }";
  return ss->str();
}

void AuthzChecker::AddToBody(const std::string &key,
                             std::shared_ptr<std::ostringstream> ss) {
  *(ss.get()) << Stringify(key.c_str()) + ": " + "{";
}
void AuthzChecker::AddToBody(const std::string &key, const std::string &value,
                             bool end, std::shared_ptr<std::ostringstream> ss) {
  *(ss.get()) << Stringify(key.c_str()) + ": " + Stringify(value.c_str());
  if (!end) {
    *(ss.get()) << ", ";
  }
}

const std::string AuthzChecker::GetReleaseName(
    std::shared_ptr<context::RequestContext> request_context) {
   return request_context->service_context()->service_name() + ":"
      + request_context->service_context()->service().apis(0).version();
}

const std::string AuthzChecker::GetReleaseUrl(
    std::shared_ptr<context::RequestContext> request_context) {
    return std::string(kFirebaseServerStaging) + "v1/projects/"
      + request_context->service_context()->project_id() + "/releases/"
      + GetReleaseName(request_context);
}

void AuthzChecker::HttpFetch(
    const std::string &url, const std::string &method,
    const std::string &request_body,
    std::function<void(Status, std::string &&)> continuation) {

  env_->LogInfo(std::string("Issue HTTP Request to url :") + url +
                 " method : " + method + " body: " + request_body);

  std::unique_ptr<HTTPRequest> request(new HTTPRequest([continuation](
      Status status, std::map<std::string, std::string> &&,
      std::string &&body) { continuation(status, std::move(body)); }));

  if (!request) {
    continuation(Status(Code::INTERNAL, "Out of memory"), "");
    return;
  }

  std::string token = GetAuthToken();
  request->set_method(method)
      .set_url(url)
      .set_auth_token(token);
  if (method != "GET") {
     request->set_header("Content-Type", "application/json")
      .set_body(request_body);
  }

  env_->RunHTTPRequest(std::move(request));
}

void CheckSecurityRules(std::shared_ptr<context::RequestContext> context,
                        std::function<void(Status status)> continuation) {
  std::shared_ptr<AuthzChecker> checker(new AuthzChecker(
      context->service_context()->env(),
      context->service_context()->service_account_token()));
  checker->Check(context, continuation);
}

}  // namespace api_manager
}  // namespace google
