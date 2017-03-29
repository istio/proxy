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

#include <string>

#include "contrib/endpoints/include/api_manager/api_manager.h"
#include "contrib/endpoints/include/api_manager/request.h"

using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

namespace {

const char kFirebaseServerStaging[] =
    "https://staging-firebaserules.sandbox.googleapis.com/";

// An AuthzChecker object is created for every incoming request. It does
// authorizaiton by calling Firebase Rules service.
class AuthzChecker : public std::enable_shared_from_this<AuthzChecker> {
 public:
  AuthzChecker(std::shared_ptr<context::RequestContext> context,
               std::function<void(Status status)> continuation);

  void Check();

 private:
  // Helper function to send a http GET request.
  void HttpFetch(const std::string &url, const std::string &request_body,
                 std::function<void(Status, std::string &&)> continuation);

  // Get Auth token for accessing Firebase Rules service.
  const std::string &GetAuthToken();

  // Request context.
  std::shared_ptr<context::RequestContext> context_;

  // Pointer to access ESP running environment.
  ApiManagerEnvInterface *env_;

  // The final continuation function.
  std::function<void(Status status)> on_done_;
};

AuthzChecker::AuthzChecker(std::shared_ptr<context::RequestContext> context,
                           std::function<void(Status status)> continuation)
    : context_(context),
      env_(context_->service_context()->env()),
      on_done_(continuation) {}

void AuthzChecker::Check() {
  // TODO: Check service config to see if "useSecurityRules" is specified.
  // If so, call Firebase Rules service TestRuleset API.
}

const std::string &AuthzChecker::GetAuthToken() {
  // TODO: Get Auth token for accessing Firebase Rules service.
  static std::string empty;
  return empty;
}

void AuthzChecker::HttpFetch(
    const std::string &url, const std::string &request_body,
    std::function<void(Status, std::string &&)> continuation) {
  std::unique_ptr<HTTPRequest> request(new HTTPRequest([continuation](
      Status status, std::map<std::string, std::string> &&,
      std::string &&body) { continuation(status, std::move(body)); }));
  if (!request) {
    continuation(Status(Code::INTERNAL, "Out of memory"), "");
    return;
  }

  request->set_method("POST")
      .set_url(url)
      .set_auth_token(GetAuthToken())
      .set_header("Content-Type", "application/json")
      .set_body(request_body);
  env_->RunHTTPRequest(std::move(request));
}

}  // namespace

void CheckSecurityRules(std::shared_ptr<context::RequestContext> context,
                        std::function<void(Status status)> continuation) {
  std::shared_ptr<AuthzChecker> authzChecker =
      std::make_shared<AuthzChecker>(context, continuation);
  authzChecker->Check();
}

}  // namespace api_manager
}  // namespace google
