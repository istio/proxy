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

#ifdef NULL_PLUGIN
namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

using envoy::api::v2::core::GrpcService;
using Envoy::Extensions::Common::Wasm::Null::Plugin::GrpcStatus;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logDebug;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logInfo;
using Envoy::Extensions::Common::Wasm::Null::Plugin::StringView;

#endif

constexpr char kGoogleStackdriverLoggingAddress[] = "logging.googleapis.com";
constexpr char kGoogleLoggingService[] = "google.logging.v2.LoggingServiceV2";
constexpr char kGoogleWriteLogEntriesMethod[] = "WriteLogEntries";
constexpr char kDefaultRootCertFile[] = "/etc/ssl/certs/ca-certificates.crt";
constexpr int kDefaultTimeoutMillisecond = 10000;

namespace Extensions {
namespace Stackdriver {
namespace Log {

ExporterImpl::ExporterImpl(RootContext* root_context,
                           const std::string& logging_service_endpoint) {
  context_ = root_context;
  Metric export_call(MetricType::Counter, "stackdriver_filter",
                     {MetricTag{"type", MetricTag::TagType::String},
                      MetricTag{"success", MetricTag::TagType::Bool}});
  auto success_counter = export_call.resolve("logging", true);
  auto failure_counter = export_call.resolve("logging", false);
  success_callback_ = [success_counter](google::protobuf::Empty&&) {
    // TODO(bianpengyuan): replace this with envoy's generic gRPC counter.
    incrementMetric(success_counter, 1);
    logDebug("successfully sent Stackdriver logging request");
  };

  failure_callback_ = [failure_counter](GrpcStatus status, StringView message) {
    // TODO(bianpengyuan): add retry.
    // TODO(bianpengyuan): replace this with envoy's generic gRPC counter.
    incrementMetric(failure_counter, 1);
    logWarn("Stackdriver logging api call error: " +
            std::to_string(static_cast<int>(status)) + std::string(message));
  };

  // Construct grpc_service for the Stackdriver gRPC call.
  GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("stackdriver_logging");
  if (logging_service_endpoint.empty()) {
    grpc_service.mutable_google_grpc()->set_target_uri(
        kGoogleStackdriverLoggingAddress);
    grpc_service.mutable_google_grpc()
        ->add_call_credentials()
        ->mutable_google_compute_engine();
    grpc_service.mutable_google_grpc()
        ->mutable_channel_credentials()
        ->mutable_ssl_credentials()
        ->mutable_root_certs()
        ->set_filename(kDefaultRootCertFile);
  } else {
    // Do not set credential if target uri is provided. This should happen in
    // test.
    grpc_service.mutable_google_grpc()->set_target_uri(
        logging_service_endpoint);
  }
  grpc_service.SerializeToString(&grpc_service_string_);
}

void ExporterImpl::exportLogs(
    const std::vector<
        std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
        requests) const {
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
