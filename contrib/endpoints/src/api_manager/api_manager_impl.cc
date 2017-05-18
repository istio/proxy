// Copyright 2016 Google Inc. All Rights Reserved.
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
//
#include <iostream>
#include <thread>

#include "contrib/endpoints/src/api_manager/api_manager_impl.h"
#include "contrib/endpoints/src/api_manager/check_workflow.h"
#include "contrib/endpoints/src/api_manager/request_handler.h"

namespace google {
namespace api_manager {

ApiManagerImpl::ApiManagerImpl(std::unique_ptr<ApiManagerEnvInterface> env,
                               const std::string &service_config,
                               const std::string &server_config)
    : global_context_(
          new context::GlobalContext(std::move(env), server_config)),
      service_config_(service_config),
      initialized_(false) {
  check_workflow_ = std::unique_ptr<CheckWorkflow>(new CheckWorkflow);
  check_workflow_->RegisterAll();
}

// Deploy these configs according to the traffic percentage.
void ApiManagerImpl::DeployConfigs(
    std::vector<std::pair<std::string, int>> &&list) {
  service_selector_.reset(new WeightedSelector(std::move(list)));
}

utils::Status ApiManagerImpl::Init() {
  if (global_context_->cloud_trace_aggregator()) {
    global_context_->cloud_trace_aggregator()->Init();
  }

  if (!service_config_.empty()) {
    // service config was defined in the nginx config
    // no need to create a config manager
    update_rollouts({{service_config_, 0}});
  } else {
    // Initialize the config manager instance
    config_manager_.reset(new ConfigManagerImpl(
        global_context_,
        [this](std::vector<std::pair<std::string, int>> &list) {
          update_rollouts(list);
        }));

    config_manager_->Init();
  }

  return utils::Status::OK;
}

void ApiManagerImpl::update_rollouts(
    const std::vector<std::pair<std::string, int>> &configs) {
  std::vector<std::pair<std::string, int>> selector;

  for (auto service_config : configs) {
    std::unique_ptr<Config> config =
        Config::Create(global_context_->env(), service_config.first);

    if (config != nullptr) {
      std::string service_name = config->service().name();

      if (global_context_->service_name().empty()) {
        global_context_->set_service_name(service_name);
      }

      if (service_name != global_context_->service_name()) {
        auto err_msg = std::string("Mismatched service name; existing: ") +
                       global_context_->service_name() + ", new: " +
                       service_name;
        global_context_->env()->LogError(err_msg);
      }

      std::string config_id = config->service().id();

      service_context_map_[config_id] =
          std::make_shared<context::ServiceContext>(global_context_,
                                                    std::move(config));

      service_context_map_[config_id]->service_control()->Init();

      selector.push_back({config_id, service_config.second});
    }
  }

  service_selector_.reset(new WeightedSelector(std::move(selector)));

  // Set the config manager instance was initialize.
  // Now API manager can accept client requests
  initialized_ = true;
  global_context_->env()->LogInfo("ApiManager has been initialized");
}

utils::Status ApiManagerImpl::Close() {
  if (global_context_->cloud_trace_aggregator()) {
    global_context_->cloud_trace_aggregator()->SendAndClearTraces();
  }

  for (auto it : service_context_map_) {
    if (it.second->service_control()) {
      it.second->service_control()->Close();
    }
  }
  return utils::Status::OK;
}

bool ApiManagerImpl::Enabled() const {
  for (const auto &it : service_context_map_) {
    if (it.second->Enabled()) {
      return true;
    }
  }
  return false;
}

const std::string &ApiManagerImpl::service_name() const {
  return global_context_->service_name();
}

const ::google::api::Service &ApiManagerImpl::service(
    const std::string &config_id) const {
  const auto &it = service_context_map_.find(config_id);
  if (it != service_context_map_.end()) {
    return it->second->service();
  }
  static ::google::api::Service empty;
  return empty;
}

utils::Status ApiManagerImpl::GetStatistics(
    ApiManagerStatistics *statistics) const {
  memset(&statistics->service_control_statistics, 0,
         sizeof(service_control::Statistics));
  for (const auto &it : service_context_map_) {
    if (it.second->service_control()) {
      service_control::Statistics stat;
      auto status = it.second->service_control()->GetStatistics(&stat);
      if (status.ok()) {
        statistics->service_control_statistics.Merge(stat);
      }
    }
  }

  return utils::Status::OK;
}

std::unique_ptr<RequestHandlerInterface> ApiManagerImpl::CreateRequestHandler(
    std::unique_ptr<Request> request_data) {
  std::string config_id = service_selector_->Select();
  return std::unique_ptr<RequestHandlerInterface>(
      new RequestHandler(check_workflow_, service_context_map_[config_id],
                         std::move(request_data)));
}

std::shared_ptr<ApiManager> ApiManagerFactory::CreateApiManager(
    std::unique_ptr<ApiManagerEnvInterface> env,
    const std::string &service_config, const std::string &server_config) {
  return std::shared_ptr<ApiManager>(
      new ApiManagerImpl(std::move(env), service_config, server_config));
}

}  // namespace api_manager
}  // namespace google
