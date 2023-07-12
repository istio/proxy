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

#include "extensions/access_log_policy/plugin.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/istio_dimensions.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"

#ifndef NULL_PLUGIN

#include "base64.h"

#else

#include "source/common/common/base64.h"
namespace proxy_wasm {
namespace null_plugin {
namespace AccessLogPolicy {
namespace Plugin {

using google::protobuf::util::JsonParseOptions;
using proxy_wasm::WasmHeaderMapType;

PROXY_WASM_NULL_PLUGIN_REGISTRY;

#endif

namespace {

bool setFilterStateValue(bool log) {
  auto r = setFilterStateStringValue(::Wasm::Common::kAccessLogPolicyKey, log ? "yes" : "no");
  if (r != WasmResult::Ok) {
    logWarn(toString(r));
    return false;
  }
  return true;
}

} // namespace

constexpr long long kDefaultLogWindowDurationNanoseconds = 43200000000000; // 12h

constexpr std::string_view kSource = "source";
constexpr std::string_view kAddress = "address";
constexpr std::string_view kConnection = "connection";
constexpr std::string_view kUriSanPeerCertificate = "uri_san_peer_certificate";
constexpr std::string_view kResponse = "response";
constexpr std::string_view kCode = "code";
constexpr std::string_view kGrpcStatus = "grpc_status";

static RegisterContextFactory register_AccessLogPolicy(CONTEXT_FACTORY(PluginContext),
                                                       ROOT_FACTORY(PluginRootContext));

bool PluginRootContext::onConfigure(size_t size) {
  initialized_ = configure(size);
  return true;
}

bool PluginRootContext::configure(size_t configuration_size) {
  auto configuration_data =
      getBufferBytes(WasmBufferType::PluginConfiguration, 0, configuration_size);
  auto configuration = configuration_data->toString();
  JsonParseOptions json_options;
  json_options.ignore_unknown_fields = true;
  const auto status = JsonStringToMessage(configuration, &config_, json_options);
  if (!status.ok()) {
    logWarn("Cannot parse AccessLog plugin configuration JSON string " + configuration + ", " +
            std::string(status.message()));
    return false;
  }

  if (config_.has_log_window_duration()) {
    log_time_duration_nanos_ =
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(config_.log_window_duration());
  } else {
    log_time_duration_nanos_ = kDefaultLogWindowDurationNanoseconds;
  }

  if (config_.max_client_cache_size() > 0) {
    max_client_cache_size_ = config_.max_client_cache_size();
  }

  return true;
}

void PluginRootContext::updateLastLogTimeNanos(const Wasm::Common::IstioDimensions& key,
                                               long long last_log_time_nanos) {
  if (int32_t(cache_.size()) > max_client_cache_size_) {
    auto it = cache_.begin();
    cache_.erase(cache_.begin(), std::next(it, max_client_cache_size_ / 4));
    logDebug(absl::StrCat("cleaned cache, new cache_size:", cache_.size()));
  }
  cache_[key] = last_log_time_nanos;
}

void PluginContext::onLog() {
  if (!rootContext()->initialized()) {
    return;
  }

  // Check if request is a failure.
  if (isRequestFailed()) {
    LOG_TRACE("Setting logging to true as we got error log");
    setFilterStateValue(true);
    return;
  }

  // If request is not a failure, check cache to see if it should be logged or
  // not, based on last time a successful request was logged for this client ip
  // and principal combination.
  std::string source_ip = "";
  getValue({kSource, kAddress}, &source_ip);
  std::string source_principal = "";
  getValue({kConnection, kUriSanPeerCertificate}, &source_principal);
  istio_dimensions_.set_downstream_ip(source_ip);
  istio_dimensions_.set_source_principal(source_principal);
  long long last_log_time_nanos = lastLogTimeNanos();
  auto cur = static_cast<long long>(getCurrentTimeNanoseconds());
  if ((cur - last_log_time_nanos) > logTimeDurationNanos()) {
    LOG_TRACE(
        absl::StrCat("Setting logging to true as its outside of log windown. SourceIp: ", source_ip,
                     " SourcePrincipal: ", source_principal, " Window: ", logTimeDurationNanos()));
    if (setFilterStateValue(true)) {
      updateLastLogTimeNanos(cur);
    }
    return;
  }

  setFilterStateValue(false);
}

bool PluginContext::isRequestFailed() {
  // Check if HTTP request is a failure.
  int64_t http_response_code = 0;
  if (getValue({kResponse, kCode}, &http_response_code) && http_response_code >= 400) {
    return true;
  }

  // Check if gRPC request is a failure.
  int64_t grpc_response_code = 0;
  if (::Wasm::Common::kGrpcContentTypes.count(
          getHeaderMapValue(WasmHeaderMapType::RequestHeaders,
                            ::Wasm::Common::kContentTypeHeaderKey)
              ->toString()) != 0 &&
      getValue({kResponse, kGrpcStatus}, &grpc_response_code) && grpc_response_code != 0) {
    return true;
  }

  return false;
}

#ifdef NULL_PLUGIN
} // namespace Plugin
} // namespace AccessLogPolicy
} // namespace null_plugin
} // namespace proxy_wasm
#endif
