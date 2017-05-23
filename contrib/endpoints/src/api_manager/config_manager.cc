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
#include "contrib/endpoints/src/api_manager/utils/marshalling.h"
#include "utils/marshalling.h"

#include "google/api/servicemanagement/v1/servicemanager.pb.h"
#include "utils/md5.h"

#include <math.h>

#include <chrono>
#include <ctime>
#include <future>
#include <iostream>
#include <limits>
#include <mutex>
#include <regex>
#include <thread>

using ::google::api::Service;
using ::google::api::servicemanagement::v1::ListServiceRolloutsResponse;
using ::google::api::servicemanagement::v1::Rollout_RolloutStatus;

namespace google {
namespace api_manager {

namespace {

const char kDelimiter[] = "\0";
const int kDelimiterLength = 1;

// Initial metadata fetch timeout (1s)
const int kInceptionFetchTimeout = 1000;
// Maximum number of retries to fetch metadata
const int kInceptionFetchRetries = 5;
// Default rollouts refresh interval in ms
const int kConfigUpdateCheckInterval = 60000;

const char kRolloutStrategyManaged[] = "managed";
// Default service management API url
const char kServiceManagementService[] =
    "https://servicemanagement.googleapis.com";
const char kServiceManagementServiceManager[] =
    "/google.api.servicemanagement.v1.ServiceManager";
// config_id validation
const char kValidConfigDateFormat[] = "%Y-%m-%d";
const char kConfigRevisionDelimeter = 'r';
// static configs for error handling
static std::vector<std::pair<std::string, int>> empty_configs;
};

ConfigManager::ConfigManager(
    std::shared_ptr<context::GlobalContext> global_context,
    ApiManagerCallbackFunction config_roollout_callback)
    : global_context_(global_context),
      config_roollout_callback_(config_roollout_callback),
      service_management_url_(kServiceManagementService),
      refresh_interval_ms_(kConfigUpdateCheckInterval) {
  if (global_context_->server_config()->has_service_management_config()) {
    // update ServiceManagement service API url
    if (!global_context_->server_config()
             ->service_management_config()
             .url()
             .empty()) {
      service_management_url_ =
          global_context_->server_config()->service_management_config().url();
    }
    // update refresh interval in ms
    if (global_context_->server_config()
            ->service_management_config()
            .refresh_interval_ms() > 0) {
      refresh_interval_ms_ = global_context_->server_config()
                                 ->service_management_config()
                                 .refresh_interval_ms();
    }
  }
}

void ConfigManager::Init() {
  if (global_context_->service_name().empty() ||
      global_context_->config_id().empty()) {
    GlobalFetchGceMetadata(global_context_, [this](utils::Status status) {
      on_fetch_metadata(status);
    });
  } else {
    on_fetch_metadata(utils::Status::OK);
  }
}

void ConfigManager::on_fetch_metadata(utils::Status status) {
  if (!status.ok()) {
    // We should not get here
    global_context_->env()->LogError("Unexpected status: " + status.ToString());
    config_roollout_callback_(utils::Status(Code::ABORTED, status.message()),
                              empty_configs);
    return;
  }

  // Update service_name
  if (global_context_->service_name().empty()) {
    global_context_->set_service_name(
        global_context_->gce_metadata()->endpoints_service_name());
  }

  // Update config_id
  if (global_context_->config_id().empty()) {
    global_context_->config_id(
        global_context_->gce_metadata()->endpoints_service_config_id());
  }

  // Call ApiManager call with Code::ABORTED, ESP will stop moving forward
  if (global_context_->service_name().empty()) {
    std::string msg = "API service name not specified in configuration files";
    global_context_->env()->LogError(msg);
    config_roollout_callback_(utils::Status(Code::ABORTED, msg), empty_configs);
    return;
  }

  // TODO(jaebong) config_id should not be empty for the first version
  // This part will be removed after the rollouts feature added
  if (global_context_->config_id().empty()) {
    std::string msg = "API config_id not specified in configuration files";
    global_context_->env()->LogError(msg);
    config_roollout_callback_(utils::Status(Code::ABORTED, msg), empty_configs);
    return;
  }

  // Fetch service account token
  GlobalFetchServiceAccountToken(global_context_, [this](utils::Status status) {
    on_fetch_auth_token(status);
  });
}

void ConfigManager::on_fetch_auth_token(utils::Status status) {
  if (!status.ok()) {
    // We should not get here
    global_context_->env()->LogError("Unexpected status: " + status.ToString());
    config_roollout_callback_(utils::Status(Code::ABORTED, status.message()),
                              empty_configs);
  }

  if (global_context_->service_account_token()) {
    // register auth token for servicemanagement services
    global_context_->service_account_token()->SetAudience(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES,
        service_management_url_ + kServiceManagementServiceManager);
  }

  // Fetch configs from the Inception
  auto fetchInfo = std::make_shared<ConfigsFetchInfo>();

  // For now, config manager has only one config_id (100% rollout)
  fetchInfo->rollouts = {{global_context_->config_id(), 0}};
  fetch_configs(fetchInfo);
}

void ConfigManager::fetch_configs(std::shared_ptr<ConfigsFetchInfo> fetchInfo) {
  // Finished fetching configs.
  if (fetchInfo->rollouts.size() <= (size_t)fetchInfo->index_ ||
      fetchInfo->index_ < 0) {
    // Failed to fetch all configs or rollouts are empty
    if (fetchInfo->rollouts.size() == 0 || fetchInfo->configs.size() == 0) {
      // first time, call the ApiManager callback function with an error
      config_roollout_callback_(
          utils::Status(Code::ABORTED, "Failed to load configs"),
          empty_configs);
      return;
    }

    // Update ApiManager
    config_roollout_callback_(utils::Status::OK, fetchInfo->configs);
    return;
  }

  HttpCallbackFunction on_fetch_done = [this, &fetchInfo](
      const utils::Status& status, const std::string& config) {
    if (status.ok()) {
      fetchInfo->configs.push_back(
          {config, fetchInfo->rollouts[fetchInfo->index_].second});
    } else {
      global_context_->env()->LogError(
          std::string("Unable to decide the config_id: " +
                      fetchInfo->rollouts[fetchInfo->index_].first));
    }

    // move on to the next config_id
    fetchInfo->index_++;
    fetch_configs(fetchInfo);
  };

  const std::string url = service_management_url_ + "/v1/services/" +
                          global_context_->service_name() + "/configs/" +
                          fetchInfo->rollouts[fetchInfo->index_].first;

  call(url, on_fetch_done);
}

void ConfigManager::call(const std::string& url, HttpCallbackFunction on_done) {
  std::unique_ptr<HTTPRequest> http_request(
      new HTTPRequest([this, url, on_done](
          utils::Status status, std::map<std::string, std::string>&& headers,
          std::string&& body) {
        if (!status.ok()) {
          global_context_->env()->LogError(
              std::string("Failed to call ") + url + ", Error: " +
              status.ToString() + ", Response body: " + body);

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
      .set_auth_token(get_auth_token())
      .set_timeout_ms(kInceptionFetchTimeout)
      .set_max_retries(kInceptionFetchRetries);

  global_context_->env()->RunHTTPRequest(std::move(http_request));
}

const std::string& ConfigManager::get_auth_token() {
  if (global_context_->service_account_token()) {
    return global_context_->service_account_token()->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES);
  } else {
    static std::string empty;
    return empty;
  }
}

}  // namespace api_manager
}  // namespace google
