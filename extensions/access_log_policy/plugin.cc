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
#include "extensions/common/node_info.pb.h"
#include "google/protobuf/util/json_util.h"

#ifndef NULL_PLUGIN

#include "base64.h"

#else

#include "common/common/base64.h"
namespace Envoy {
namespace Extensions {
namespace Wasm {
namespace AccessLogPolicy {
namespace Plugin {
using namespace ::Envoy::Extensions::Common::Wasm::Null::Plugin;
using NullPluginRootRegistry =
    ::Envoy::Extensions::Common::Wasm::Null::NullPluginRootRegistry;
using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::Status;

NULL_PLUGIN_ROOT_REGISTRY;

#endif

namespace {

bool setFilterStateValue(bool log) {
  auto r = setFilterStateStringValue(::Wasm::Common::kAccessLogPolicyKey,
                                     log ? "yes" : "no");
  if (r != WasmResult::Ok) {
    logWarn(toString(r));
    return false;
  }
  return true;
}

}  // namespace

constexpr absl::Duration kDefaultLogWindowDurationNanoseconds =
    absl::Hours(12);  // 12h

constexpr StringView kDestination = "destination";
constexpr StringView kAddress = "address";
constexpr StringView kConnection = "connection";
constexpr StringView kUriSanPeerCertificate = "uri_san_peer_certificate";

static RegisterContextFactory register_AccessLogPolicy(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

bool PluginRootContext::onConfigure(size_t) {
  if (::Wasm::Common::TrafficDirection::Inbound !=
      ::Wasm::Common::getTrafficDirection()) {
    throw EnvoyException("ASM Acess Logging Policy is an inbound filter only.");
  }
  WasmDataPtr configuration = getConfiguration();
  JsonParseOptions json_options;
  Status status =
      JsonStringToMessage(configuration->toString(), &config_, json_options);
  if (status != Status::OK) {
    logWarn("Cannot parse Stackdriver plugin configuration JSON string " +
            configuration->toString() + ", " + status.message().ToString());
    return false;
  }

  if (config_.has_log_window_duration()) {
    log_time_duration_nanos_ = absl::Nanoseconds(
        ::google::protobuf::util::TimeUtil::DurationToNanoseconds(
            config_.log_window_duration()));
  } else {
    log_time_duration_nanos_ = kDefaultLogWindowDurationNanoseconds;
  }

  return true;
}

void PluginContext::onLog() {
  // Check if request is a failure.
  int64_t response_code = 0;
  // TODO(gargnupur): Add check for gRPC status too.
  getValue({"response", "code"}, &response_code);
  // If request is a failure, log it.
  if (response_code != 200) {
    GOOGLE_LOG(INFO)<<"Reached  here setting filterstate true, response code 200";
    setFilterStateValue(true);
    return;
  }

  // If request is not a failure, check cache to see if it should be logged or
  // not, based on last time a successful request was logged for this client ip
  // and principal combination.
  std::string downstream_ip = "";
  getStringValue({kDestination, kAddress}, &downstream_ip);
  std::string source_principal = "";
  getStringValue({kConnection, kUriSanPeerCertificate}, &source_principal);
  istio_dimensions_.set_downstream_ip(downstream_ip);
  istio_dimensions_.set_source_principal(source_principal);
  GOOGLE_LOG(INFO)<<"downstream_ip: "<<downstream_ip;
  GOOGLE_LOG(INFO)<<"source_principal: "<<source_principal;
  absl::Time last_log_time_nanos = lastLogTimeNanos();
  auto cur = absl::Now();
  if ((cur - last_log_time_nanos) > logTimeDurationNanos()) {
    GOOGLE_LOG(INFO)<<"Reached  here setting filterstate true, time to log";
    if (setFilterStateValue(true)) {
      updateLastLogTimeNanos(cur);
    }
    return;
  }

  GOOGLE_LOG(INFO)<<"Reached  here setting filterstate false";
  setFilterStateValue(false);
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace AccessLogPolicy
}  // namespace Wasm
}  // namespace Extensions
}  // namespace Envoy
#endif
