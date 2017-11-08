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

#include "src/envoy/mixer/config.h"
#include "include/attributes_builder.h"

using ::istio::mixer::v1::Attributes;
using ::istio::mixer_client::AttributesBuilder;
using ::istio::mixer::v1::config::client::MixerControlConfig;
using ::istio::mixer::v1::config::client::MixerFilterConfig;

namespace Envoy {
namespace Http {
namespace Mixer {
namespace {

// The Json object name for static attributes.
const std::string kMixerAttributes("mixer_attributes");

// The Json object name to specify attributes which will be forwarded
// to the upstream istio proxy.
const std::string kForwardAttributes("forward_attributes");

// The Json object name for quota name and amount.
const std::string kQuotaName("quota_name");
const std::string kQuotaAmount("quota_amount");

// The Json object name to disable check cache, quota cache and report batch
const std::string kDisableCheckCache("disable_check_cache");
const std::string kDisableQuotaCache("disable_quota_cache");
const std::string kDisableReportBatch("disable_report_batch");

const std::string kNetworkFailPolicy("network_fail_policy");
const std::string kDisableTcpCheckCalls("disable_tcp_check_calls");

void ReadStringMap(const Json::Object& json, const std::string& name,
                   Attributes* attributes) {
  if (json.hasObject(name)) {
    json.getObject(name)->iterate(
        [attributes](const std::string& key, const Json::Object& obj) -> bool {
          AttributesBuilder(attributes).AddIpOrString(key, obj.asString());
          return true;
        });
  }
}

}  // namespace

void MixerConfig::Load(const Json::Object& json) {
  ReadStringMap(json, kMixerAttributes,
                filter_config.mutable_mixer_attributes());
  ReadStringMap(json, kForwardAttributes,
                filter_config.mutable_forward_attributes());

  // Default is open, unless it specifically set to "close"
  filter_config.set_network_fail_policy(MixerFilterConfig::FAIL_OPEN);
  if (json.hasObject(kNetworkFailPolicy) &&
      json.getString(kNetworkFailPolicy) == "close") {
    filter_config.set_network_fail_policy(MixerFilterConfig::FAIL_CLOSE);
  }

  filter_config.set_disable_check_cache(
      json.getBoolean(kDisableCheckCache, false));
  filter_config.set_disable_quota_cache(
      json.getBoolean(kDisableQuotaCache, false));
  filter_config.set_disable_report_batch(
      json.getBoolean(kDisableReportBatch, false));
  filter_config.set_disable_tcp_check_calls(
      json.getBoolean(kDisableTcpCheckCalls, false));

  AttributesBuilder builder(filter_config.mutable_mixer_attributes());
  if (json.hasObject(kQuotaName)) {
    builder.AddString("quota.name", json.getString(kQuotaName));
  }
  if (json.hasObject(kQuotaAmount)) {
    builder.AddInt64("quota.amount", std::stoi(json.getString(kQuotaAmount)));
  }
}

void MixerConfig::CreateLegacyConfig(
    bool disable_check, bool disable_report,
    const std::map<std::string, std::string>& attributes,
    MixerControlConfig* config) {
  config->set_enable_mixer_check(!disable_check);
  config->set_enable_mixer_report(!disable_report);

  AttributesBuilder builder(config->mutable_mixer_attributes());
  for (const auto& it : attributes) {
    builder.AddIpOrString(it.first, it.second);
  }
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
