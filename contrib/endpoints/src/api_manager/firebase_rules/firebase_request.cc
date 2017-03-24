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
#include "contrib/endpoints/src/api_manager/firebase_rules/firebase_request.h"
#include "contrib/endpoints/src/api_manager/utils/marshalling.h"
#include "contrib/endpoints/src/api_manager/utils/url_util.h"
#include "google/protobuf/util/message_differencer.h"

#include <algorithm>
#include <sstream>
using ::google::api_manager::utils::Status;
using ::google::api_manager::proto::TestRulesetResponse;
using ::google::protobuf::util::MessageDifferencer;
using ::google::protobuf::Map;

namespace google {
namespace api_manager {
namespace firebase_rules {

namespace {

const char kToken[] = "token";
const char kAuth[] = "auth";
const char kPath[] = "path";
const char kMethod[] = "method";
const char kHttpGetMethod[] = "GET";
const char kHttpPostMethod[] = "POST";
const char kHttpHeadMethod[] = "HEAD";
const char kHttpOptionsMethod[] = "OPTIONS";
const char kHttpDeleteMethod[] = "DELETE";
const char kFirebaseCreateMethod[] = "create";
const char kFirebaseGetMethod[] = "get";
const char kFirebaseDeleteMethod[] = "delete";
const char kFirebaseUpdateMethod[] = "update";
const char kV1[] = "/v1";
const char kTestQuery[] = ":test?alt=json";

void SetProtoValue(const std::string &key,
                   const ::google::protobuf::Value &value,
                   ::google::protobuf::Value *head) {
  ::google::protobuf::Struct *s = head->mutable_struct_value();
  Map<std::string, google::protobuf::Value> *fields = s->mutable_fields();
  (*fields)[key] = value;
}

// Convert HTTP method to Firebase specific method.
std::string GetOperation(const std::string &httpMethod) {
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
}

// Constructor
FirebaseRequest::FirebaseRequest(
    const std::string &ruleset_name, ApiManagerEnvInterface *env,
    std::shared_ptr<context::RequestContext> context)
    : env_(env),
      context_(context),
      ruleset_name_(ruleset_name),
      service_name_(context->service_context()->service_name()),
      firebase_server_(
          context->service_context()->config()->GetFirebaseServer()),
      current_status_(Status::OK),
      is_done_(false),
      next_request_(nullptr) {
  firebase_http_request_.url =
      firebase_server_ + kV1 + "/" + ruleset_name + kTestQuery;
  firebase_http_request_.method = kHttpPostMethod;
  firebase_http_request_.token_type =
      auth::ServiceAccountToken::JWT_TOKEN_FOR_FIREBASE;
  external_http_request_.token_type =
      auth::ServiceAccountToken::JWT_TOKEN_FOR_AUTHORIZATION_SERVICE;

  // Update the first request to be sent which is the TestRulesetRequest
  // request.
  SetStatus(UpdateRulesetRequestBody(RepeatedPtrField<FunctionCall>()));
  if (!current_status_.ok()) {
    return;
  }

  next_request_ = &firebase_http_request_;
}

bool FirebaseRequest::IsDone() { return is_done_; }

HttpRequest FirebaseRequest::GetHttpRequest() {
  if (IsDone()) {
    return HttpRequest();
  }

  if (next_request_ == nullptr) {
    SetStatus(Status(Code::INTERNAL, "Internal state in error"));
    return HttpRequest();
  }

  return *next_request_;
}

Status FirebaseRequest::RequestStatus() { return current_status_; }

void FirebaseRequest::UpdateResponse(const std::string &body) {
  if (IsDone()) {
    env_->LogError(
        "Receive a response body when no HTTP request is outstanding");
    return;
  }

  if (next_request_ == nullptr) {
    env_->LogError(
        "Received a response when there is no request set"
        "and when IsDone is false."
        " Looks like a code bug...");
    SetStatus(Status(Code::INTERNAL,
                     "Internal state error while processing Http request"));
    return;
  }

  Status status = Status::OK;

  // If the previous request was firebase request, then process its response.
  // Otherwise, it is the response for external HTTP request.
  if (next_request_ == &firebase_http_request_) {
    status = ProcessTestRulesetResponse(body);
  } else {
    status = ProcessFunctionCallResponse(body);
  }

  if (status.ok()) {
    status = SetNextRequest();
  }

  SetStatus(status);
  return;
}

void FirebaseRequest::SetStatus(const Status &status) {
  if (!status.ok() && !is_done_) {
    current_status_ = status;
    is_done_ = true;
  }
}

// Create the TestRulesetRequest body.
Status FirebaseRequest::UpdateRulesetRequestBody(
    const RepeatedPtrField<FunctionCall> &function_calls) {
  proto::TestRulesetRequest request;
  auto test_case = request.mutable_test_suite()->add_test_cases();
  test_case->set_expectation(proto::TestCase::ALLOW);

  ::google::protobuf::Value token;
  ::google::protobuf::Value claims;
  ::google::protobuf::Value path;
  ::google::protobuf::Value method;

  Status status = utils::JsonToProto(context_->auth_claims(), &claims);
  if (!status.ok()) {
    return status;
  }

  auto *variables = test_case->mutable_request()->mutable_struct_value();
  auto *fields = variables->mutable_fields();

  path.set_string_value(context_->request()->GetRequestPath());
  (*fields)[kPath] = path;

  method.set_string_value(
      GetOperation(context_->request()->GetRequestHTTPMethod()));
  (*fields)[kMethod] = method;

  SetProtoValue(kToken, claims, &token);
  (*fields)[kAuth] = token;

  for (auto func_call : function_calls) {
    status = AddFunctionMock(&request, func_call);
    if (!status.ok()) {
      return status;
    }
  }

  std::string body;
  status = utils::ProtoToJson(request, &body, utils::JsonOptions::DEFAULT);
  if (status.ok()) {
    env_->LogDebug(std::string("FIREBASE REQUEST BODY = ") + body);
    firebase_http_request_.body = body;
  }

  return status;
}

Status FirebaseRequest::ProcessTestRulesetResponse(const std::string &body) {
  Status status = utils::JsonToProto(body, &response_);
  if (!status.ok()) {
    return status;
  }

  // If the state is SUCCESS, then we don't need to do any further processing.
  if (response_.test_results(0).state() ==
      TestRulesetResponse::TestResult::SUCCESS) {
    is_done_ = true;
    next_request_ = nullptr;
    return Status::OK;
  }

  // Check that the test results size is 1 since we always send a single test
  // case.
  if (response_.test_results_size() != 1) {
    std::ostringstream oss;
    oss << "Received TestResultsetResponse with size = "
        << response_.test_results_size() << " expecting only 1 test result";

    env_->LogError(oss.str());
    return Status(Code::INTERNAL, "Unexpected TestResultsetResponse");
  }

  bool allFunctionsProcessed = true;

  // Iterate over all the function calls and make sure that the function calls
  // are well formed.
  for (auto func_call : response_.test_results(0).function_calls()) {
    status = CheckFuncCallArgs(func_call);
    if (!status.ok()) {
      return status;
    }
    allFunctionsProcessed &= Find(func_call) != funcs_with_result_.end();
  }

  // Since all the functions have a response and the state is FAILURE, this
  // means Unauthorized access to the resource.
  if (allFunctionsProcessed) {
    std::string message = "Unauthorized Access";
    if (response_.test_results(0).debug_messages_size() > 0) {
      std::ostringstream oss;
      for (std::string msg : response_.test_results(0).debug_messages()) {
        oss << msg << " ";
      }
      message = oss.str();
    }

    return Status(Code::PERMISSION_DENIED, message);
  }

  func_call_iter_ = response_.test_results(0).function_calls().begin();
  return Status::OK;
}

std::vector<std::pair<FunctionCall, std::string>>::const_iterator
FirebaseRequest::Find(const FunctionCall &func_call) {
  return std::find_if(funcs_with_result_.begin(), funcs_with_result_.end(),
                      [func_call](std::tuple<FunctionCall, std::string> item) {
                        return MessageDifferencer::Equals(std::get<0>(item),
                                                          func_call);
                      });
}

Status FirebaseRequest::ProcessFunctionCallResponse(const std::string &body) {
  if (IsDone() || AllFunctionCallsProcessed()) {
    return Status(Code::INTERNAL,
                  "No external function calls present."
                  " But received a response. Possible code bug");
  }

  funcs_with_result_.emplace_back(*func_call_iter_, body);
  func_call_iter_++;
  return Status::OK;
}

// Sets the next HTTP request that should be issued.
Status FirebaseRequest::SetNextRequest() {
  if (IsDone()) {
    next_request_ = nullptr;
    return current_status_;
  }

  Status status = Status::OK;

  // While there are more functions that should be processed, check if the HTTP
  // response for the function is already buffered. Set the next HTTP request if
  // we find a new function and break.
  while (!AllFunctionCallsProcessed()) {
    if (Find(*func_call_iter_) == funcs_with_result_.end()) {
      auto call = *func_call_iter_;
      external_http_request_.url = call.args(0).string_value();
      external_http_request_.method = call.args(1).string_value();
      std::string body;
      status =
          utils::ProtoToJson(call.args(2), &body, utils::JsonOptions::DEFAULT);
      if (status.ok()) {
        external_http_request_.body = body;
        next_request_ = &external_http_request_;
      }
      break;
    }

    func_call_iter_++;
  }

  // If All functions are processed, then issue a TestRulesetRequest.
  if (AllFunctionCallsProcessed()) {
    next_request_ = &firebase_http_request_;
    return UpdateRulesetRequestBody(response_.test_results(0).function_calls());
  }

  return status;
}

Status FirebaseRequest::CheckFuncCallArgs(const FunctionCall &func) {
  if (func.function().empty()) {
    return Status(Code::INVALID_ARGUMENT, "No function name provided");
  }

  // We only support functions that call with three argument: HTTP URL, HTTP
  // method and body. The body can be empty
  if (func.args_size() < 2 || func.args_size() > 3) {
    std::ostringstream os;
    os << func.function() << " Require 2 or 3 arguments. But has "
       << func.args_size();
    return Status(Code::INVALID_ARGUMENT, os.str());
  }

  if (func.args(0).kind_case() != google::protobuf::Value::kStringValue ||
      func.args(1).kind_case() != google::protobuf::Value::kStringValue) {
    return Status(
        Code::INVALID_ARGUMENT,
        std::string(func.function() + " Arguments 1 and 2 should be strings"));
  }

  if (!utils::IsHttpRequest(func.args(0).string_value())) {
    return Status(
        Code::INVALID_ARGUMENT,
        func.function() + " The first argument should be a HTTP request");
  }

  if (std::string(func.args(1).string_value()).empty()) {
    return Status(
        Code::INVALID_ARGUMENT,
        func.function() + " argument 2 [HTTP METHOD] cannot be emtpy");
  }

  return Status::OK;
}

bool FirebaseRequest::AllFunctionCallsProcessed() {
  return func_call_iter_ == response_.test_results(0).function_calls().end();
}

Status FirebaseRequest::AddFunctionMock(proto::TestRulesetRequest *request,
                                        const FunctionCall &func_call) {
  if (Find(func_call) == funcs_with_result_.end()) {
    return Status(Code::INTERNAL,
                  std::string("Cannot find body for function call") +
                      func_call.function());
  }

  auto *func_mock = request->mutable_test_suite()
                        ->mutable_test_cases(0)
                        ->add_function_mocks();

  func_mock->set_function(func_call.function());
  for (auto arg : func_call.args()) {
    auto *toAdd = func_mock->add_args()->mutable_exact_value();
    *toAdd = arg;
  }

  ::google::protobuf::Value result_json;
  Status status =
      utils::JsonToProto(std::get<1>(*Find(func_call)), &result_json);
  if (!status.ok()) {
    env_->LogError(std::string("Error creating protobuf from request body") +
                   status.ToString());
    return status;
  }

  *(func_mock->mutable_result()->mutable_value()) = result_json;
  return Status::OK;
}

}  // namespace firebase_rules
}  // namespace api_manager
}  // namespace google
