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

#include "service_context.h"

#include "include/istio/utils/attribute_names.h"
#include "src/istio/control/http/attributes_builder.h"

using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::config::client::ServiceConfig;

namespace istio {
namespace control {
namespace http {

ServiceContext::ServiceContext(std::shared_ptr<ClientContext> client_context,
                               const ServiceConfig *config)
    : client_context_(client_context) {
  if (config) {
    service_config_.reset(new ServiceConfig(*config));
  }
  BuildParsers();
}

void ServiceContext::BuildParsers() {
  if (!service_config_) {
    return;
  }
  // Build quota parser
  for (const auto &quota : service_config_->quota_spec()) {
    quota_parsers_.push_back(
        ::istio::quota_config::ConfigParser::Create(quota));
  }
}

// Add static mixer attributes.
void ServiceContext::AddStaticAttributes(
    ::istio::mixer::v1::Attributes *attributes) const {
  client_context_->AddLocalNodeAttributes(attributes);

  if (client_context_->config().has_mixer_attributes()) {
    attributes->MergeFrom(client_context_->config().mixer_attributes());
  }
  if (service_config_ && service_config_->has_mixer_attributes()) {
    attributes->MergeFrom(service_config_->mixer_attributes());
  }
}

// Inject a header that contains the static forwarded attributes.
void ServiceContext::InjectForwardedAttributes(
    HeaderUpdate *header_update) const {
  Attributes attributes;

  client_context_->AddLocalNodeForwardAttribues(&attributes);

  if (client_context_->config().has_forward_attributes()) {
    attributes.MergeFrom(client_context_->config().forward_attributes());
  }
  if (service_config_ && service_config_->has_forward_attributes()) {
    attributes.MergeFrom(service_config_->forward_attributes());
  }

  if (!attributes.attributes().empty()) {
    AttributesBuilder::ForwardAttributes(attributes, header_update);
  }
}

// Add quota requirements from quota configs.
void ServiceContext::AddQuotas(
    ::istio::mixer::v1::Attributes *attributes,
    std::vector<::istio::quota_config::Requirement> &quotas) const {
  for (const auto &parser : quota_parsers_) {
    parser->GetRequirements(*attributes, &quotas);
  }
}

}  // namespace http
}  // namespace control
}  // namespace istio
