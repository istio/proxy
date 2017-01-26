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
#ifndef API_MANAGER_CHECK_SECURITY_RULES_H_
#define API_MANAGER_CHECK_SECURITY_RULES_H_

#include "contrib/endpoints/include/api_manager/api_manager.h"
#include "contrib/endpoints/src/api_manager/auth/service_account_token.h"
#include "contrib/endpoints/src/api_manager/context/request_context.h"
#include "contrib/endpoints/include/api_manager/utils/status.h"

#include <string>
using ::google::api_manager::utils::Status;

namespace google {
namespace api_manager {

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
  void CheckAuth(std::string ruleset_id,
                 std::shared_ptr<context::RequestContext> context,
                 std::function<void(Status status)> continuation);

  // Parse the respose for GET RELEASE API call
  std::pair<Status, std::string> ParseReleaseResponse(std::string *json_str);

  // Parses the response for the TEST API call
  Status ParseTestResponse(std::shared_ptr<context::RequestContext> context,
                           std::string *json_str);

  // Builds the request body for the TESP API call.
  std::string BuildTestRequestBody(
      std::shared_ptr<context::RequestContext> context);

  void AddToBody(const std::string &key,
                 std::shared_ptr<std::ostringstream> ss);

  void AddToBody(const std::string &key, const std::string &value,
                 bool end, std::shared_ptr<std::ostringstream> ss);

  // Get the release name to used in Firebase API call
  const std::string GetReleaseName(
      std::shared_ptr<context::RequestContext> request_context);

  // Get the URL for the Release request.
  const std::string GetReleaseUrl(
      std::shared_ptr<context::RequestContext> request_context);

  // Invoke the HTTP call
  void HttpFetch(const std::string &url, const std::string &method,
                 const std::string &request_body,
                 std::function<void(Status, std::string &&)> continuation);


  // Get the auth token for Firebase service
  const std::string &GetAuthToken() {
    return sa_token_->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_FIREBASE);
  }

  // Get Firebase specific operation Id based on the http Method.
  std::string GetOperation(std::string httpMethod);

  const std::string Stringify(const char *s) {
    return std::string("\"") + s + "\"";
  }

  std::shared_ptr<AuthzChecker> GetPtr() { return shared_from_this(); }

  ApiManagerEnvInterface *env_;
  auth::ServiceAccountToken *sa_token_;
};

// This function checks security rules for a given request.
// It is called by CheckWorkflow class when processing a request.
void CheckSecurityRules(std::shared_ptr<context::RequestContext> context,
                        std::function<void(utils::Status status)> continuation);

}  // namespace api_manager
}  // namespace google

#endif  // API_MANAGER_CHECK_SECURITY_RULES_H_
