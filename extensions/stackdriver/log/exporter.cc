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
#include "extensions/stackdriver/common/metrics.h"

#ifdef NULL_PLUGIN

#include "envoy/config/core/v3/grpc_service.pb.h"

namespace proxy_wasm {
namespace null_plugin {

using envoy::config::core::v3::GrpcService;

#endif

constexpr char kGoogleLoggingService[] = "google.logging.v2.LoggingServiceV2";
constexpr char kGoogleWriteLogEntriesMethod[] = "WriteLogEntries";
constexpr int kDefaultTimeoutMillisecond = 10000;

namespace Extensions {
namespace Stackdriver {
namespace Log {

ExporterImpl::ExporterImpl(
    RootContext* root_context,
    const ::Extensions::Stackdriver::Common::StackdriverStubOption& stub_option) {
  context_ = root_context;
  auto success_counter = Common::newExportCallMetric("logging", true);
  auto failure_counter = Common::newExportCallMetric("logging", false);
  success_callback_ = [this, success_counter](size_t) {
    incrementMetric(success_counter, 1);
    LOG_DEBUG("successfully sent Stackdriver logging request");
    in_flight_export_call_ -= 1;
    if (in_flight_export_call_ < 0) {
      LOG_WARN("in flight report call should not be negative");
    }
    if (in_flight_export_call_ <= 0 && is_on_done_) {
      proxy_done();
    }
  };

  failure_callback_ = [this, failure_counter](GrpcStatus status) {
    // TODO(bianpengyuan): add retry.
    incrementMetric(failure_counter, 1);
    LOG_WARN("Stackdriver logging api call error: " + std::to_string(static_cast<int>(status)) +
             getStatus().second->toString());
    in_flight_export_call_ -= 1;
    if (in_flight_export_call_ < 0) {
      LOG_WARN("in flight report call should not be negative");
    }
    if (in_flight_export_call_ <= 0 && is_on_done_) {
      proxy_done();
    }
  };

  // Construct grpc_service for the Stackdriver gRPC call.
  GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("stackdriver_logging");
  if (stub_option.enable_log_compression) {
    (*grpc_service.mutable_google_grpc()
          ->mutable_channel_args()
          ->mutable_args())[GRPC_COMPRESSION_CHANNEL_DEFAULT_ALGORITHM]
        .set_int_value(GRPC_COMPRESS_GZIP);
  }
  buildEnvoyGrpcService(stub_option, &grpc_service);
  grpc_service.SerializeToString(&grpc_service_string_);
}

void ExporterImpl::exportLogs(
    const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>& requests,
    bool is_on_done) {
  is_on_done_ = is_on_done;
  HeaderStringPairs initial_metadata;
  for (const auto& req : requests) {
    auto result = context_->grpcSimpleCall(
        grpc_service_string_, kGoogleLoggingService, kGoogleWriteLogEntriesMethod, initial_metadata,
        *req, kDefaultTimeoutMillisecond, success_callback_, failure_callback_);
    if (result != WasmResult::Ok) {
      LOG_WARN("failed to make stackdriver logging export call");
      break;
    }
    in_flight_export_call_ += 1;
  }
}

} // namespace Log
} // namespace Stackdriver
} // namespace Extensions

#ifdef NULL_PLUGIN
} // namespace null_plugin
} // namespace proxy_wasm
#endif
