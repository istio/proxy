/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "src/istio/control/http/client_context.h"
#include "include/istio/utils/attribute_names.h"

using ::istio::mixer::v1::Attributes_AttributeValue;
using ::istio::mixer::v1::config::client::ServiceConfig;
using ::istio::utils::AttributeName;
using ::istio::utils::CreateLocalAttributes;

namespace istio {
namespace control {
namespace http {
const char* kReporterOutbound = "outbound";

namespace {

// isOutbound returns true if this is an outbound listener configuration.
// It relies on pilot setting context.reporter.kind == outbound;
static bool isOutbound(
    const ::istio::mixer::v1::config::client::HttpClientConfig& config) {
  bool outbound = false;
  const auto& attributes_map = config.mixer_attributes().attributes();
  const auto it = attributes_map.find(AttributeName::kContextReporterKind);
  if (it != attributes_map.end()) {
    const Attributes_AttributeValue& value = it->second;
    if (kReporterOutbound == value.string_value()) {
      outbound = true;
    }
  }
  return outbound;
}

}  // namespace

ClientContext::ClientContext(const Controller::Options& data)
    : ClientContextBase(data.config.transport(), data.env),
      config_(data.config),
      service_config_cache_size_(data.service_config_cache_size),
      outbound_(isOutbound(data.config)) {
  CreateLocalAttributes(data.local_node, &local_attributes_);
}

ClientContext::ClientContext(
    std::unique_ptr<::istio::mixerclient::MixerClient> mixer_client,
    const ::istio::mixer::v1::config::client::HttpClientConfig& config,
    int service_config_cache_size,
    ::istio::utils::LocalAttributes& local_attributes, bool outbound)
    : ClientContextBase(std::move(mixer_client)),
      config_(config),
      service_config_cache_size_(service_config_cache_size),
      local_attributes_(local_attributes),
      outbound_(outbound) {}

const std::string& ClientContext::GetServiceName(
    const std::string& service_name) const {
  if (service_name.empty()) {
    return config_.default_destination_service();
  }
  const auto& config_map = config_.service_configs();
  auto it = config_map.find(service_name);
  if (it == config_map.end()) {
    return config_.default_destination_service();
  }
  return service_name;
}

// Get the service config by the name.
const ServiceConfig* ClientContext::GetServiceConfig(
    const std::string& service_name) const {
  const auto& config_map = config_.service_configs();
  auto it = config_map.find(service_name);
  if (it != config_map.end()) {
    return &it->second;
  }
  return nullptr;
}

void ClientContext::AddLocalNodeAttributes(
    ::istio::mixer::v1::Attributes* request) const {
  if (outbound_) {
    request->MergeFrom(local_attributes_.outbound);
  } else {
    request->MergeFrom(local_attributes_.inbound);
  }
}

void ClientContext::AddLocalNodeForwardAttribues(
    ::istio::mixer::v1::Attributes* request) const {
  if (outbound_) {
    request->MergeFrom(local_attributes_.forward);
  }
}

}  // namespace http
}  // namespace control
}  // namespace istio
