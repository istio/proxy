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
#include "contrib/endpoints/src/api_manager/config_manager_impl.h"
#include "contrib/endpoints/src/api_manager/utils/marshalling.h"
#include "utils/marshalling.h"

#include "google/api/servicemanagement/v1/servicemanager.pb.h"
#include "utils/md5.h"

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>

namespace google {
namespace api_manager {

namespace {

const char kDelimiter[] = "\0";
const int kDelimiterLength = 1;

// Initial metadata fetch timeout (1s)
const int kInceptionFetchTimeout = 1000;
// Maximum number of retries to fetch metadata
const int kInceptionFetchRetries = 5;

const int kConfigUpdateCheckInterval = 60000;

const char kValidateConfigId[] =
    "^(2)\\d\\d\\d[- \\/.](0[1-9]|1[012])[- \\/.]"
    "(0[1-9]|[12][0-9]|3[01])r(0|[1-9][0-9]*)$";

const char kServiceManagementService[] =
    "https://servicemanagement.googleapis.com";

const char kServiceManagementServiceManager[] =
    "/google.api.servicemanagement.v1.ServiceManager";

const char kRolloutStrategyManaged[] = "managed";

const char kRolloutStrategyFixed[] = "fixed";

bool isValidConfigId(const std::string& config_id) {
  return !config_id.empty();
}
};

ConfigManagerImpl::ConfigManagerImpl(
    std::shared_ptr<context::GlobalContext> global_context,
    std::function<void(std::vector<std::pair<std::string, int>>&)>
        config_roollout_callback)
    : global_context_(global_context),
      rollouts_check_timer_(nullptr),
      config_roollout_callback_(config_roollout_callback) {}

// TODO(jaebong):: This function must be synchronous
void ConfigManagerImpl::Init() {
  std::function<void(Status)> fetch_account_token_done = [this](Status status) {
    if (global_context_->service_name().empty()) {
      global_context_->set_service_name(
          global_context_->gce_metadata()->endpoints_service_name());
    }

    if (global_context_->config_id().empty()) {
      global_context_->config_id(
          global_context_->gce_metadata()->endpoints_service_config_id());
    }

    if (global_context_->service_account_token()) {
      // register auth token for servicemanagement services
      global_context_->service_account_token()->SetAudience(
          auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES,
          std::string(kServiceManagementService) +
              kServiceManagementServiceManager);
    }

    // TODO(jaebong): to test list fetch. should be removed.
    global_context_->config_id("");

    if (isValidConfigId(global_context_->config_id())) {
      // config_id is specified in the configuration
      std::vector<std::pair<std::string, int>> rollouts = {
          {global_context_->config_id(), 100}};
      std::vector<std::pair<std::string, int>> configs;
      // fetch configs from servicemanagement apis
      if (check_rollout_required(rollouts)) {
        fetch_configs_from_rollouts(rollouts, 0, configs);
        update_rollout_signature(rollouts);
      }
    } else if (global_context_->config_id().empty()) {
      // start periodic timer when the rollout_strategy is "managed"
      if (global_context_->rollout_strategy().compare(
              std::string(kRolloutStrategyManaged)) == 0) {
        // start periodic timer to check rollouts
        std::function<void()> config_refresh_callback = [this]() { onTimer(); };
        rollouts_check_timer_ = global_context_->env()->StartPeriodicTimer(
            std::chrono::milliseconds(kConfigUpdateCheckInterval), config_refresh_callback);
      }

      // trigger initial update regardless of rollout_strategy
      onTimer();
    } else {
      // unable to decide the config_id
      global_context_->env()->LogError(
          std::string("Unable to get the config_id"));
    }
  };

  std::function<void(Status)> fetch_gce_metadata_done =
      [fetch_account_token_done, this](Status status) {
        GlobalFetchServiceAccountToken(global_context_,
                                       fetch_account_token_done);
      };

  GlobalFetchGceMetadata(global_context_, fetch_gce_metadata_done);
}

void ConfigManagerImpl::onTimer() {
  // Fetch rollouts from servicemanagement api
  std::function<void(const utils::Status&, const std::string&)> callback =
      [this](const utils::Status& status, const std::string& body) {
        if (status.ok()) {
          ListServiceRolloutsResponse listServiceRolloutsResponse;
          utils::Status status = utils::JsonToProto(
              body, (::google::protobuf::Message*)&listServiceRolloutsResponse);

          if (status.ok()) {
            std::string rollout_id;

            for (auto rollout : listServiceRolloutsResponse.rollouts()) {
              if (rollout.status() ==
                  ::google::api::servicemanagement::v1::Rollout_RolloutStatus::
                      Rollout_RolloutStatus_SUCCESS) {
                rollout_id = rollout.rollout_id();

                std::vector<std::pair<std::string, int>> rollouts;
                for (auto percentage :
                     rollout.traffic_percent_strategy().percentages()) {
                  rollouts.push_back(
                      {percentage.first, int(percentage.second + 0.5)});
                }

                // fetch configs from rollouts
                if (check_rollout_required(rollouts)) {
                  std::vector<std::pair<std::string, int>> configs;
                  fetch_configs_from_rollouts(rollouts, 0, configs);
                  update_rollout_signature(rollouts);
                }
                break;
              }
            }

            if (rollout_id.empty()) {
              if (!listServiceRolloutsResponse.next_page_token().empty()) {
                // TODO(jaebong): there is second page. Need to fetch rollout
                // from the second page
                // fetch_list_service_rollouts(
                //    listServiceRolloutsResponse.next_page_token(), callback);
              } else {
                global_context_->env()->LogError("No active rollout");
              }
            }
          } else {
            global_context_->env()->LogError(
                std::string("Failed to handle rollout list , Error: ") +
                status.ToString() + ", Response body: " + body);
          }
        } else {
          global_context_->env()->LogError(
              std::string("Failed to download rollout list, Error: ") +
              status.ToString() + ", Response body: " + body);
        }
      };

  fetch_rollouts("", callback);
}

bool ConfigManagerImpl::check_rollout_required(
    std::vector<std::pair<std::string, int>> rollouts) {
  std::string current_roollout_signature = rollout_signature(rollouts);
  if (current_roollout_signature_.compare(current_roollout_signature) != 0) {
    current_roollout_signature_ = current_roollout_signature;
    return true;
  }

  return false;
}

void ConfigManagerImpl::update_rollout_signature(
    std::vector<std::pair<std::string, int>> rollouts) {
  current_roollout_signature_ = rollout_signature(rollouts);
}

const std::string ConfigManagerImpl::rollout_signature(
    std::vector<std::pair<std::string, int>> rollouts) {
  ::google::service_control_client::MD5 hasher;

  for (const auto& rollout : rollouts) {
    hasher.Update(rollout.first);
    hasher.Update(kDelimiter, kDelimiterLength);
    hasher.Update(std::to_string(rollout.second));
    hasher.Update(kDelimiter, kDelimiterLength);
  }

  return hasher.Digest();
}

void ConfigManagerImpl::fetch_configs_from_rollouts(
    std::vector<std::pair<std::string, int>> rollouts, std::size_t index,
    std::vector<std::pair<std::string, int>> configs) {
  if (rollouts.size() <= index || index < 0) {
    // Finished fetching configs.
    if (config_roollout_callback_) {
      // Call config_roollout_callback_
      config_roollout_callback_(configs);
    }
    return;
  }

  ConfigFetchCallback on_fetch_done = [this, rollouts, configs, index](
      const utils::Status& status, const std::string& config) {
    if (status.ok()) {
      std::vector<std::pair<std::string, int>> copied_configs(configs);
      copied_configs.push_back({config, rollouts[index].second});
      // move on to the next config_id
      fetch_configs_from_rollouts(rollouts, index + 1, copied_configs);
    } else {
      // unable to fetch the selected config_id.
      // TODO(jaebong): is this fatal?
      global_context_->env()->LogError(std::string(
          "Unable to decide the config_id: " + rollouts[index].first));
    }
  };

  fetch_config(rollouts[index].first, on_fetch_done);
}

/*
== [done] Just like service_name, create a config_id in global_context, copy the
one from server_config if nginx set it.
== [done] check if service_name is empty, call fetch_metadata to get it.
== [done] check if config_id is empty, call fetch _metadata to get it
== [done] call fetch_metadata_token to get access token

== if config_id is valid, call inception to get that config, and call set_config
and deploy_config to use it.
== otherwise, call inception to get latest rollout, from the rollout to get
configs and call inception to get configs and use the
 */

void ConfigManagerImpl::fetch_rollouts(
    const std::string& page_token,
    std::function<void(const utils::Status&, const std::string&)>
        on_config_list_done) {
  const std::string url =
      std::string(kServiceManagementService) + "/v1/services/" +
      global_context_->service_name() + "/rollouts" +
      (!page_token.empty() ? ("?page_token=" + page_token) : "");

  call(url, on_config_list_done);
}

void ConfigManagerImpl::fetch_config(const std::string& config_id,
                                     ConfigFetchCallback on_fetch_config_done) {
  const std::string url = std::string(kServiceManagementService) +
                          "/v1/services/" + global_context_->service_name() +
                          "/configs/" + config_id;

  call(url, on_fetch_config_done);
}

void ConfigManagerImpl::call(
    const std::string& url,
    std::function<void(const utils::Status&, const std::string&)> on_done) {
  std::unique_ptr<HTTPRequest> http_request(new HTTPRequest([this, url,
                                                             on_done](
      utils::Status status, std::map<std::string, std::string>&& headers,
      std::string&& body) {

    if (status.ok()) {
      // Call AddConfig callback on ApiManager
      on_done(status, body);
    } else {
      global_context_->env()->LogError(std::string("Failed to call ") + url +
                                       ", Error: " + status.ToString() +
                                       ", Response body: " + body);

      // Handle NGX error as opposed to pass-through error code
      if (status.code() < 0) {
        status =
            Status(Code::UNAVAILABLE, "Failed to connect to service control");
      } else {
        status = Status(
            Code::UNAVAILABLE,
            "Service management request failed with HTTP response code " +
                std::to_string(status.code()));
      }
    }
  }));

  http_request->set_url(url)
      .set_method("GET")
      .set_auth_token(GetAuthToken())
      .set_timeout_ms(kInceptionFetchTimeout)
      .set_max_retries(kInceptionFetchRetries);

  global_context_->env()->RunHTTPRequest(std::move(http_request));
}

const std::string& ConfigManagerImpl::GetAuthToken() {
  if (global_context_->service_account_token()) {
    return global_context_->service_account_token()->GetAuthToken(
        auth::ServiceAccountToken::JWT_TOKEN_FOR_SERVICEMANAGEMENT_SERVICES);
  } else {
    static std::string empty;
    return empty;
  }
}

}  // namespace service_control_client
}  // namespace google
