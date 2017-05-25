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
#ifndef API_MANAGER_CONFIG_MANAGER_H_
#define API_MANAGER_CONFIG_MANAGER_H_

#include "contrib/endpoints/include/api_manager/http_request.h"
#include "contrib/endpoints/src/api_manager/context/global_context.h"
#include "contrib/endpoints/src/api_manager/fetch_metadata.h"
#include "contrib/endpoints/src/api_manager/proto/server_config.pb.h"
#include "contrib/endpoints/src/api_manager/service_management_fetch.h"

#include "google/api/servicemanagement/v1/servicemanager.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace api_manager {

namespace {

// ApiManagerCallbackFunction is the callback provided by ApiManager.
// ConfigManager calls the callback after the service config download
//
// status
//  - Code::OK        Config manager was successfully initialized
//  - Code::ABORTED   Fatal error
//  - Code::UNKNOWN   Config manager was not initialized yet
// configs - pairs of ServiceConfig in text and rollout percentages
typedef std::function<void(const utils::Status& status,
                           std::vector<std::pair<std::string, int>>& configs)>
    ApiManagerCallbackFunction;

// Data structure to fetch configs from rollouts
struct ConfigsFetchInfo {
  ConfigsFetchInfo() : index(0) {}
  // config_ids to be fetched and rollouts percentages
  std::vector<std::pair<std::string, int>> rollouts;
  // fetched ServiceConfig and rollouts percentages
  std::vector<std::pair<std::string, int>> configs;

  // index to be fetched
  int index;
};

}  // namespace anonymous

// Manages configuration downloading
class ConfigManager {
 public:
  ConfigManager(std::shared_ptr<context::GlobalContext> global_context,
                ApiManagerCallbackFunction config_roollout_callback);

  virtual ~ConfigManager(){};

 public:
  // Initialize the instance
  void Init();

 private:
  // Fetch ServiceConfig details from the latest successful rollouts
  // https://goo.gl/I2nD4M
  void fetch_configs(std::shared_ptr<ConfigsFetchInfo> fetchInfo);

  // Handle metadata fetch done
  void on_fetch_metadata(utils::Status status);

  // Handle auth token fetch done
  void on_fetch_auth_token(utils::Status status);

  // Global context provided by ApiManager
  std::shared_ptr<context::GlobalContext> global_context_;

  // ApiManager updated callback
  ApiManagerCallbackFunction config_roollout_callback_;

  // Service Management API base url
  std::string service_management_host_;
  // Rollouts refresh check interval in ms
  int refresh_interval_ms_;
};

}  // namespace api_manager
}  // namespace google
#endif  // API_MANAGER_CONFIG_MANAGER_H_
