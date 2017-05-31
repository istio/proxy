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
      config_loading_status_(
          utils::Status(Code::UNKNOWN, "Not initialized yet")) {
  check_workflow_ = std::unique_ptr<CheckWorkflow>(new CheckWorkflow);
  check_workflow_->RegisterAll();
}

bool ApiManagerImpl::AddConfig(const std::string &service_config,
                               std::string *config_id) {
  std::unique_ptr<Config> config =
      Config::Create(global_context_->env(), service_config);
  if (config == nullptr) {
    global_context_->env()->LogError(std::string("Invalid service config: ") +
                                     service_config);
    return false;
  }

  std::string service_name = config->service().name();
  if (global_context_->service_name().empty()) {
    global_context_->set_service_name(service_name);
  } else {
    if (service_name != global_context_->service_name()) {
      auto err_msg = std::string("Mismatched service name; existing: ") +
                     global_context_->service_name() + ", new: " + service_name;
      global_context_->env()->LogError(err_msg);
      return false;
    }
  }

  *config_id = config->service().id();

  auto context_service = std::make_shared<context::ServiceContext>(
      global_context_, std::move(config));
  context_service->service_control()->Init();
  service_context_map_[*config_id] = context_service;

  return true;
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
    std::string config_id;
    if (AddConfig(service_config_, &config_id)) {
      DeployConfigs({{config_id, 100}});

      config_loading_status_ = utils::Status::OK;
    } else {
      config_loading_status_ =
          utils::Status(Code::ABORTED, "Invalid service config");
    }
    return config_loading_status_;
  }

  config_manager_.reset(new ConfigManager(
      global_context_,
      [this](const utils::Status &status,
             const std::vector<std::pair<std::string, int>> &configs) {
        if (status.ok()) {
          std::vector<std::pair<std::string, int>> rollouts;

          for (auto item : configs) {
            std::string config_id;
            if (AddConfig(item.first, &config_id)) {
              rollouts.push_back({config_id, item.second});
            }
          }

          if (rollouts.size() == 0) {
            config_loading_status_ =
                utils::Status(Code::ABORTED, "Invalid service config");
            return;
          }

          DeployConfigs(std::move(rollouts));
        }
        config_loading_status_ = status;
      }));
  config_manager_->Init();

  return utils::Status::OK;
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
