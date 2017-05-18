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

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

// rpc ListServiceConfigs "/v1/services/{service_name}/configs"
using ::google::api::servicemanagement::v1::ListServiceConfigsRequest;
using ::google::api::servicemanagement::v1::ListServiceConfigsResponse;
// rpc GetServiceConfig "/v1/services/{service_name}/configs/{config_id}"
using ::google::api::servicemanagement::v1::GetServiceConfigRequest;
using ::google::api::Service;
using ::google::api_manager::proto::ServerConfig;
// rpc ListServiceConfigs "/v1/services/{service_name}/rollouts"
using ::google::api::servicemanagement::v1::ListServiceRolloutsResponse;

namespace google {
namespace api_manager {

typedef std::function<void(const utils::Status& status, const std::string&)>
    ConfigFetchCallback;

class ConfigManagerImpl : public ConfigManager {
 public:
  ConfigManagerImpl(
      std::shared_ptr<context::GlobalContext> global_context,
      std::function<void(std::vector<std::pair<std::string, int>>&)>
          config_roollout_callback);

  virtual ~ConfigManagerImpl(){};

  // ConfigManagerImpl is neither copyable nor movable.
  ConfigManagerImpl(const ConfigManagerImpl&) = delete;
  ConfigManagerImpl& operator=(const ConfigManagerImpl&) = delete;

 public:
  void Init() override;
  void onTimer() override;

 private:
  void fetch_rollouts(
      const std::string& page_token,
      std::function<void(const utils::Status&, const std::string&)>
          on_config_list_done);

  void fetch_config(const std::string& config_id,
                    ConfigFetchCallback on_service_config_done);

  void fetch_configs_from_rollouts(
      std::vector<std::pair<std::string, int>> rollouts, std::size_t index,
      std::vector<std::pair<std::string, int>> output);

  void call(
      const std::string& url,
      std::function<void(const utils::Status&, const std::string&)> on_done);

  const std::string& GetAuthToken();

  const std::string rollout_signature(
      std::vector<std::pair<std::string, int>> rollouts);

  bool check_rollout_required(std::vector<std::pair<std::string, int>> rollouts);
  void update_rollout_signature(std::vector<std::pair<std::string, int>> rollouts);

  std::shared_ptr<context::GlobalContext> global_context_;
  std::unique_ptr<PeriodicTimer> rollouts_check_timer_;
  std::function<void(std::vector<std::pair<std::string, int>>&)>
      config_roollout_callback_;

  std::string current_roollout_signature_;
};

}  // namespace service_control_client
}  // namespace google
#endif  // API_MANAGER_CONFIG_MANAGER_IMPL_H_
