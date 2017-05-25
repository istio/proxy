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
#include "contrib/endpoints/src/api_manager/service_management_fetch.h"

namespace google {
namespace api_manager {

namespace {
// Default rollouts refresh interval in ms
const int kConfigUpdateCheckInterval = 60000;

const char kRolloutStrategyManaged[] = "managed";

// static configs for error handling
static std::vector<std::pair<std::string, int>> empty_configs;
}  // namespace anonymous

ConfigManager::ConfigManager(
    std::shared_ptr<context::GlobalContext> global_context,
    ApiManagerCallbackFunction config_rollout_callback)
    : global_context_(global_context),
      config_rollout_callback_(config_rollout_callback),
      refresh_interval_ms_(kConfigUpdateCheckInterval) {
  if (global_context_->server_config()->has_service_management_config()) {
    // update refresh interval in ms
    if (global_context_->server_config()
            ->service_management_config()
            .refresh_interval_ms() > 0) {
      refresh_interval_ms_ = global_context_->server_config()
                                 ->service_management_config()
                                 .refresh_interval_ms();
    }
  }

  service_management_fetch_.reset(new ServiceManagementFetch(global_context));
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
    config_rollout_callback_(utils::Status(Code::ABORTED, status.message()),
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
    config_rollout_callback_(utils::Status(Code::ABORTED, msg), empty_configs);
    return;
  }

  // TODO(jaebong) config_id should not be empty for the first version
  // This part will be removed after the rollouts feature added
  if (global_context_->config_id().empty()) {
    std::string msg = "API config_id not specified in configuration files";
    global_context_->env()->LogError(msg);
    config_rollout_callback_(utils::Status(Code::ABORTED, msg), empty_configs);
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
    config_rollout_callback_(utils::Status(Code::ABORTED, status.message()),
                             empty_configs);
  }

  // Fetch configs from the Inception
  auto fetchInfo = std::make_shared<ConfigsFetchInfo>();

  // For now, config manager has only one config_id (100% rollout)
  fetchInfo->rollouts = {{global_context_->config_id(), 100}};
  fetch_configs(fetchInfo);
}

// Fetch configs from rollouts. fetchInfo has rollouts and fetched configs
void ConfigManager::fetch_configs(std::shared_ptr<ConfigsFetchInfo> fetchInfo) {
  // Finished fetching configs.
  if (fetchInfo->IsCompleted()) {
    // Failed to fetch all configs or rollouts are empty
    if (fetchInfo->rollouts.size() == 0 || fetchInfo->configs.size() == 0) {
      // first time, call the ApiManager callback function with an error
      config_rollout_callback_(
          utils::Status(Code::ABORTED, "Failed to load configs"),
          empty_configs);
      return;
    }

    // Update ApiManager
    config_rollout_callback_(utils::Status::OK, fetchInfo->configs);
    return;
  }

  service_management_fetch_->GetConfig(
      fetchInfo->rollouts[fetchInfo->index].first,
      [this, &fetchInfo](const utils::Status& status,
                         const std::string& config) {
        if (status.ok()) {
          fetchInfo->configs.push_back(
              {std::move(config),
               fetchInfo->rollouts[fetchInfo->index].second});
        } else {
          global_context_->env()->LogError(std::string(
              "Unable to download ServiceConfig for the config_id: " +
              fetchInfo->rollouts[fetchInfo->index].first));
        }

        // move on to the next config_id
        fetchInfo->Next();
        fetch_configs(fetchInfo);
      });
}

}  // namespace api_manager
}  // namespace google
