/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "extensions/attributegen/plugin.h"

#include "absl/strings/ascii.h"
#include "extensions/common/util.h"
#include "extensions/attributegen/proxy_expr.h"
#include "google/protobuf/util/time_util.h"

using google::protobuf::util::TimeUtil;

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace AttributeGen {

bool PluginRootContext::onConfigure(size_t) {
  std::unique_ptr<WasmData> configuration = getConfiguration();
  // Parse configuration JSON string.
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    LOG_WARN(absl::StrCat("Cannot parse plugin configuration JSON string ",
                          configuration->toString()));
    return false;
  }

  if (!::Wasm::Common::extractLocalNodeFlatBuffer(&local_node_info_)) {
    LOG_WARN("cannot parse local node metadata ");
    return false;
  }
  outbound_ = ::Wasm::Common::TrafficDirection::Outbound ==
              ::Wasm::Common::getTrafficDirection();

  if (outbound_) {
    peer_metadata_id_key_ = ::Wasm::Common::kUpstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kUpstreamMetadataKey;
  } else {
    peer_metadata_id_key_ = ::Wasm::Common::kDownstreamMetadataIdKey;
    peer_metadata_key_ = ::Wasm::Common::kDownstreamMetadataKey;
  }

  debug_ = config_.debug();
  use_host_header_fallback_ = !config_.disable_host_header_fallback();

  cleanupExpressions();

  long long tcp_report_duration_milis = kDefaultTCPReportDurationMilliseconds;
  if (config_.has_tcp_reporting_duration()) {
    tcp_report_duration_milis =
        ::google::protobuf::util::TimeUtil::DurationToMilliseconds(
            config_.tcp_reporting_duration());
  }
  proxy_set_tick_period_milliseconds(tcp_report_duration_milis);

  return true;
}

void PluginRootContext::cleanupExpressions() {
  for (uint32_t token : expressions_) {
    exprDelete(token);
  }
  expressions_.clear();
  input_expressions_.clear();
  for (uint32_t token : int_expressions_) {
    exprDelete(token);
  }
  int_expressions_.clear();
}

Optional<size_t> PluginRootContext::addStringExpression(
    const std::string& input) {
  auto it = input_expressions_.find(input);
  if (it == input_expressions_.end()) {
    uint32_t token = 0;
    if (createExpression(input, &token) != WasmResult::Ok) {
      LOG_WARN(absl::StrCat("Cannot create an expression: " + input));
      return {};
    }
    size_t result = expressions_.size();
    input_expressions_[input] = result;
    expressions_.push_back(token);
    return result;
  }
  return it->second;
}

Optional<uint32_t> PluginRootContext::addIntExpression(
    const std::string& input) {
  uint32_t token = 0;
  if (createExpression(input, &token) != WasmResult::Ok) {
    LOG_WARN(absl::StrCat("Cannot create a value expression: " + input));
    return {};
  }
  int_expressions_.push_back(token);
  return token;
}

#ifdef NULL_PLUGIN
NullPluginRegistry* context_registry_{};

class AttributeGenFactory : public NullVmPluginFactory {
 public:
  std::string name() const override { return "envoy.wasm.attributegen"; }

  std::unique_ptr<NullVmPlugin> create() const override {
    return std::make_unique<NullPlugin>(context_registry_);
  }
};

static Registry::RegisterFactory<AttributeGenFactory, NullVmPluginFactory> register_;
#endif

}  // namespace AttributeGen

#ifdef NULL_PLUGIN
// WASM_EPILOG
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
