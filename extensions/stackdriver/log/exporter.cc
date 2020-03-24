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

#include "extensions/stackdriver/log/exporter.h"

#include "extensions/stackdriver/common/constants.h"

#ifdef NULL_PLUGIN
namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

using envoy::config::core::v3::GrpcService;
using Envoy::Extensions::Common::Wasm::Null::Plugin::GrpcStatus;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logDebug;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logInfo;
using Envoy::Extensions::Common::Wasm::Null::Plugin::StringView;

#endif

constexpr char kGoogleLoggingService[] = "google.logging.v2.LoggingServiceV2";
constexpr char kGoogleWriteLogEntriesMethod[] = "WriteLogEntries";
constexpr int kDefaultTimeoutMillisecond = 10000;

namespace Extensions {
namespace Stackdriver {
namespace Log {

ExporterImpl::ExporterImpl(
    RootContext* root_context,
    const ::Extensions::Stackdriver::Common::StackdriverStubOption&
        stub_option) {
  context_ = root_context;
  Metric export_call(MetricType::Counter, "stackdriver_filter",
                     {MetricTag{"type", MetricTag::TagType::String},
                      MetricTag{"success", MetricTag::TagType::Bool}});
  success_callback_ = [this](size_t) {
    logDebug("successfully sent Stackdriver logging request");
    if (is_on_done_) {
      proxy_done();
    }
  };

  failure_callback_ = [this](GrpcStatus status) {
    // TODO(bianpengyuan): add retry.
    logWarn("Stackdriver logging api call error: " +
            std::to_string(static_cast<int>(status)) +
            getStatus().second->toString());
    if (is_on_done_) {
      proxy_done();
    }
  };

  // Construct grpc_service for the Stackdriver gRPC call.
  GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("stackdriver_logging");
  buildEnvoyGrpcService(stub_option, &grpc_service);
  grpc_service.SerializeToString(&grpc_service_string_);
}

void ExporterImpl::exportLogs(
    const std::vector<std::unique_ptr<
        const google::logging::v2::WriteLogEntriesRequest>>& requests,
    bool is_on_done) {
  is_on_done_ = is_on_done;
  for (const auto& req : requests) {
    context_->grpcSimpleCall(grpc_service_string_, kGoogleLoggingService,
                             kGoogleWriteLogEntriesMethod, *req,
                             kDefaultTimeoutMillisecond, success_callback_,
                             failure_callback_);
  }
}

}  // namespace Log
}  // namespace Stackdriver
}  // namespace Extensions

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
