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
#ifndef API_MANAGER_CONFIG_MANAGER_IMPL_H_
#define API_MANAGER_CONFIG_MANAGER_IMPL_H_

#include "contrib/endpoints/include/api_manager/config_manager.h"
#include "contrib/endpoints/include/api_manager/http_request.h"
#include "contrib/endpoints/src/api_manager/context/global_context.h"
#include "contrib/endpoints/src/api_manager/fetch_metadata.h"
#include "contrib/endpoints/src/api_manager/proto/server_config.pb.h"

#include "google/api/servicemanagement/v1/servicemanager.pb.h"
#include "google/protobuf/stubs/status.h"

namespace google {
namespace api_manager {

typedef std::function<void(const utils::Status&, const std::string&)>
    ApiCallbackFunction;

class ConfigManagerImpl : public ConfigManager {
 public:
  ConfigManagerImpl(
      std::shared_ptr<context::GlobalContext> global_context,
      std::function<void(const utils::Status&,
                         std::vector<std::pair<std::string, int>>&)>
          config_roollout_callback);

  virtual ~ConfigManagerImpl(){};

 public:
  void Init() override;

 private:
  // Fetch ServiceConfig details from the latest successful rollouts
  // https://goo.gl/I2nD4M
  void fetch_configs(std::vector<std::pair<std::string, int>> rollouts,
                     std::size_t index,
                     std::vector<std::pair<std::string, int>> output);

  // Send HTTP GET request to ServiceManagement API
  void call(const std::string& url, ApiCallbackFunction on_done);

  // Generate Auth token for ServiceManagement API
  const std::string& get_auth_token();

  // Returns callback to trigger fetching metadata
  std::function<void(utils::Status)> get_fetch_metadata_callback();

  // Returns callback to trigger fetching auth token
  std::function<void(utils::Status)> get_fetch_auth_token_callback();

  // Internal callback functions
  std::function<void(utils::Status)> on_fetch_metadata_callback_;
  std::function<void(utils::Status)> on_fetch_auth_token_callback_;

  // Global context provided by ApiManager
  std::shared_ptr<context::GlobalContext> global_context_;

  std::function<void(const utils::Status&,
                     std::vector<std::pair<std::string, int>>&)>
      config_roollout_callback_;

  // Service Management API base url
  std::string service_management_url_;
  // Rollouts refresh check interval in ms
  int refresh_interval_ms_;
};

}  // namespace api_manager
}  // namespace google
#endif  // API_MANAGER_CONFIG_MANAGER_IMPL_H_
