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

#include <iostream>

namespace google {
namespace api_manager {

namespace {

// Initial metadata fetch timeout (1s)
const int kInceptionFetchTimeout = 1000;
// Maximum number of retries to fetch metadata
const int kInceptionFetchRetries = 5;

// HTTP request callback
typedef std::function<void(const utils::Status&, const std::string&)>
    HttpCallbackFunction;

const std::string& get_auth_token(
    std::shared_ptr<context::GlobalContext> context) {
  if (context->service_account_token()) {
    return context->service_account_token()->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES);
  } else {
    static std::string empty;
    return empty;
  }
}

void call(std::shared_ptr<context::GlobalContext> context,
          const std::string& url, HttpCallbackFunction on_done) {
  std::unique_ptr<HTTPRequest> http_request(
      new HTTPRequest([context, url, on_done](
          utils::Status status, std::map<std::string, std::string>&& headers,
          std::string&& body) {
        if (!status.ok()) {
          context->env()->LogError(std::string("Failed to call ") + url +
                                   ", Error: " + status.ToString() +
                                   ", Response body: " + body);

          // Handle NGX error as opposed to pass-through error code
          if (status.code() < 0) {
            status = utils::Status(Code::UNAVAILABLE,
                                   "Failed to connect to service management");
          } else {
            status = utils::Status(
                Code::UNAVAILABLE,
                "Service management request failed with HTTP response code " +
                    std::to_string(status.code()));
          }
        }

        on_done(status, body);
      }));

  http_request->set_url(url)
      .set_method("GET")
      .set_auth_token(get_auth_token(context))
      .set_timeout_ms(kInceptionFetchTimeout)
      .set_max_retries(kInceptionFetchRetries);

  context->env()->RunHTTPRequest(std::move(http_request));
}
}

void FetchServiceManagementConfig(
    std::shared_ptr<context::GlobalContext> context, std::string config_id,
    std::function<void(utils::Status, const std::string& config)> callback) {
  // context->server_config()->service_management_config().url() was set by
  // the constructor of ConfigManager class
  const std::string url =
      context->server_config()->service_management_config().url() +
      "/v1/services/" + context->service_name() + "/configs/" + config_id;
  call(context, url, callback);
}

}  // namespace api_manager
}  // namespace google
