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

#ifndef FIREBASE_RULES_FIREBASE_REQUEST_H_
#define FIREBASE_RULES_FIREBASE_REQUEST_H_

#include <string>
#include <utility>
#include <vector>
#include "contrib/endpoints/include/api_manager/utils/status.h"
#include "contrib/endpoints/src/api_manager/context/request_context.h"
#include "contrib/endpoints/src/api_manager/proto/security_rules.pb.h"

using ::google::api_manager::utils::Status;
using TestRulesetResponse = ::google::api_manager::proto::TestRulesetResponse;
using FunctionCall = TestRulesetResponse::TestResult::FunctionCall;
using ::google::protobuf::RepeatedPtrField;

namespace google {
namespace api_manager {
namespace firebase_rules {

struct HttpRequest {
  std::string url;
  std::string method;
  std::string body;
  auth::ServiceAccountToken::JWT_TOKEN_TYPE token_type;
};

// A FirebaseRequest object understands the various http requests that need
// to be generated as a part of the TestRuleset request and response cycle.
// Here is the intented use of this code:
// FirebaseRequest request(...);
// while(!request.IsDone()) {
//  std::string url, method, body;
//
//  /* The following is not a valid C++ statement. But written so the reader can
//  get a general idea ... */
//
//  (url, method, body, token_type) = request.GetHttpRequest();
//  std::string body = InvokeHttpRequest(url, method, body,
//                                       GetToken(token_type));
//  updateResponse(body);
// }
//
// if (request.RequestStatus.ok()) {
//  .... ALLOW .....
// } else {
//  .... DENY .....
// }
class FirebaseRequest {
 public:
  // Constructor.
  FirebaseRequest(const std::string &ruleset_name, ApiManagerEnvInterface *env,
                  std::shared_ptr<context::RequestContext> context);

  // If the firebase Request calling can be terminated.
  bool IsDone();

  // Get the request status. This request status is only valid if IsDone is
  // true.
  Status RequestStatus();

  // This call should be invoked to get the next http request to execute.
  HttpRequest GetHttpRequest();

  // The response for previous HttpRequest.
  void UpdateResponse(const std::string &body);

 private:
  Status UpdateRulesetRequestBody(
      const RepeatedPtrField<FunctionCall> &func_calls);
  Status ProcessTestRulesetResponse(const std::string &body);
  Status ProcessFunctionCallResponse(const std::string &body);
  Status CheckFuncCallArgs(const FunctionCall &func);
  Status AddFunctionMock(proto::TestRulesetRequest *request,
                         const FunctionCall &func_call);
  void SetStatus(const Status &status);
  Status SetNextRequest();
  bool AllFunctionCallsProcessed();
  std::vector<std::pair<FunctionCall, std::string>>::const_iterator Find(
      const FunctionCall &func_call);

  ApiManagerEnvInterface *env_;
  std::shared_ptr<context::RequestContext> context_;
  std::string ruleset_name_;
  std::string service_name_;
  std::string firebase_server_;
  Status current_status_;
  bool is_done_;
  std::vector<std::pair<FunctionCall, std::string>> funcs_with_result_;
  RepeatedPtrField<FunctionCall>::const_iterator func_call_iter_;
  TestRulesetResponse response_;
  HttpRequest *next_request_;
  HttpRequest firebase_http_request_;
  HttpRequest external_http_request_;
};

}  // namespace firebase_rules
}  // namespace api_manager
}  // namespace google

#endif  // FIREBASE_RULES_REQUEST_HELPER_H_
