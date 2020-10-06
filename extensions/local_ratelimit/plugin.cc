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

#include "extensions/local_ratelimit/plugin.h"

#include "absl/strings/str_cat.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"

#ifndef NULL_PLUGIN

#include "proxy_wasm_intrinsics.h"

#else
#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
namespace LocalRateLimit {
namespace Plugin {

using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;
using proxy_wasm::WasmHeaderMapType;

PROXY_WASM_NULL_PLUGIN_REGISTRY;

#endif

#include "contrib/proxy_expr.h"

static RegisterContextFactory register_LocalRateLimit(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

bool PluginRootContext::onConfigure(size_t size) {
  initialized_ = configure(size);
  return true;
}

bool PluginRootContext::configure(size_t configuration_size) {
  auto configuration_data = getBufferBytes(WasmBufferType::PluginConfiguration,
                                           0, configuration_size);
  auto configuration = configuration_data->toString();
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  Status status = JsonStringToMessage(configuration, &config_, json_options);
  if (status != Status::OK) {
    LOG_WARN("Cannot parse Local RateLimit plugin configuration JSON string " +
            configuration + ", " + status.message().ToString());
    return false;
  }

  for(auto quota_config: config_.quota_configs()){
    const std::string& domain = quota_config.domain();
    if(domain.empty()){
      continue;
    }

    uint32_t max_tokens = quota_config.max_tokens();
    setSharedData(domain+"_tokens", std::to_string(max_tokens));
    auto cur = static_cast<long int>(getCurrentTimeNanoseconds());
    setSharedData(domain+"_lastAccess",std::to_string(cur));

    for(auto descriptor: quota_config.descriptors()){
      uint32_t token;
      if(WasmResult::Ok != createExpression(descriptor.first, &token)){
        LOG_TRACE(absl::StrCat("Could not create expression for ", descriptor.first));
        continue;
      }
      input_expressions_[descriptor.first]=token;
    }
  }

  return true;
}


FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t, bool) {
  if (!rootContext()->initialized()) {
      return FilterHeadersStatus::Continue;;
    }
  for(auto quota_config: rootContext()->config().quota_configs()){
    const std::string& domain = quota_config.domain();
    if(domain.empty()){
      continue;
    }

    bool found_match=true;;
    for(auto descriptor: quota_config.descriptors()){
      std::string value;
      if (!evaluateExpression(rootContext()->input_expressions().at(descriptor.first),
                            &value)){
        LOG_TRACE(absl::StrCat("Could not evaluate expression: ",descriptor.first));
        found_match = false;
        break;
      }
      if(value != descriptor.second){
        found_match = false;
        break;
      }
    }

    // If a match is found, then check if quota is left for this request.
    if(found_match){
      WasmDataPtr tokens_left;
      if (WasmResult::Ok != getSharedData(domain+"_tokens", &tokens_left)) {
              continue;
      }
      int32_t tokens = std::stoul(tokens_left->toString(),0);
      // Deduct one from the existing tokens for the current request.
      tokens --;
      // Update window if needed.
      auto cur = static_cast<long int>(getCurrentTimeNanoseconds());
      WasmDataPtr lastAccess;
      if (WasmResult::Ok != getSharedData(domain+"_lastAccess", &lastAccess)) {
        continue;
      }
      if(cur - std::stoll(lastAccess->toString(),0) > ::google::protobuf::util::TimeUtil::DurationToNanoseconds(quota_config.fill_interval())){
        uint32_t max_tokens = quota_config.max_tokens();
        uint32_t tokens_per_fill = quota_config.has_tokens_per_fill()?quota_config.tokens_per_fill().value():1;
        tokens = std::min(max_tokens,tokens + tokens_per_fill);
        setSharedData(domain+"_lastAccess",std::to_string(cur));
      }
      // No tokens left for this request. Send Resource Exhausted Error.
      if(tokens < 0){
        sendLocalResponse(429, "LocalRateLimit: Resource Exhausted", "",{}, GrpcStatus::ResourceExhausted);
        return FilterHeadersStatus::StopIteration;
      } else {
        setSharedData(domain+"_tokens",std::to_string(tokens));
      }
      // If a match was found, break the loop.
      break;
    }
  }
  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace LocalRateLimit
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
