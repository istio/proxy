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
#include "extensions/stackdriver/common/utils.h"

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

constexpr char kGoogleStackdriverLoggingAddress[] = "logging.googleapis.com";
constexpr char kGoogleLoggingService[] = "google.logging.v2.LoggingServiceV2";
constexpr char kGoogleWriteLogEntriesMethod[] = "WriteLogEntries";
constexpr int kDefaultTimeoutMillisecond = 10000;

namespace Extensions {
namespace Stackdriver {
namespace Log {

namespace {

// StackdriverLoggingHandler is used to inject x-goog-user-project header into
// gRPC initial metadata. This is a work around since gRPC library is not yet
// able to add that header automatically based on quota project from STS token.
// This should not be needed after https://github.com/grpc/grpc/issues/21225 is
// ready.
class StackdriverLoggingHandler
    : public GrpcCallHandler<google::logging::v2::WriteLogEntriesResponse> {
 public:
  StackdriverLoggingHandler(uint32_t success_counter, uint32_t failure_counter,
                            const std::string& project_id)
      : GrpcCallHandler(),
        success_counter_(success_counter),
        failure_counter_(failure_counter),
        project_id_(project_id) {}

  void onCreateInitialMetadata(uint32_t /* headers */) override {
    addHeaderMapValue(
        HeaderMapType::GrpcCreateInitialMetadata,
        ::Extensions::Stackdriver::Common::kGoogleUserProjectHeaderKey,
        project_id_);
  }

  void onSuccess(size_t /*body_size*/) override {
    // TODO(bianpengyuan): replace this with envoy's generic gRPC counter.
    incrementMetric(success_counter_, 1);
    logDebug("successfully sent Stackdriver logging request");
  }

  void onFailure(GrpcStatus status) override {
    // TODO(bianpengyuan): add retry.
    // TODO(bianpengyuan): replace this with envoy's generic gRPC counter.
    incrementMetric(failure_counter_, 1);
    logWarn("Stackdriver logging api call error: " +
            std::to_string(static_cast<int>(status)) +
            getStatus().second->toString());
  }

 private:
  uint32_t success_counter_;
  uint32_t failure_counter_;
  const std::string& project_id_;
};

}  // namespace

ExporterImpl::ExporterImpl(RootContext* root_context,
                           const std::string& logging_service_endpoint,
                           const std::string& project_id,
                           const std::string& sts_port,
                           const std::string& test_token_file,
                           const std::string& test_root_pem_file) {
  context_ = root_context;
  Metric export_call(MetricType::Counter, "stackdriver_filter",
                     {MetricTag{"type", MetricTag::TagType::String},
                      MetricTag{"success", MetricTag::TagType::Bool}});
  success_counter_ = export_call.resolve("logging", true);
  failure_counter_ = export_call.resolve("logging", false);
  project_id_ = project_id;

  // Construct grpc_service for the Stackdriver gRPC call.
  GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("stackdriver_logging");

  grpc_service.mutable_google_grpc()->set_target_uri(
      logging_service_endpoint.empty() ? kGoogleStackdriverLoggingAddress
                                       : logging_service_endpoint);
  if (sts_port.empty()) {
    // Security token exchange is not enabled. Use default GCE credential.
    grpc_service.mutable_google_grpc()
        ->add_call_credentials()
        ->mutable_google_compute_engine();
  } else {
    ::Extensions::Stackdriver::Common::setSTSCallCredentialOptions(
        grpc_service.mutable_google_grpc()
            ->add_call_credentials()
            ->mutable_sts_service(),
        sts_port,
        test_token_file.empty()
            ? ::Extensions::Stackdriver::Common::kSTSSubjectTokenPath
            : test_token_file);
  }

  grpc_service.mutable_google_grpc()
      ->mutable_channel_credentials()
      ->mutable_ssl_credentials()
      ->mutable_root_certs()
      ->set_filename(
          test_root_pem_file.empty()
              ? ::Extensions::Stackdriver::Common::kDefaultRootCertFile
              : test_root_pem_file);

  grpc_service.SerializeToString(&grpc_service_string_);
}

void ExporterImpl::exportLogs(
    const std::vector<
        std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&
        requests) const {
  for (const auto& req : requests) {
    auto handler = std::make_unique<StackdriverLoggingHandler>(
        success_counter_, failure_counter_, project_id_);
    context_->grpcCallHandler(grpc_service_string_, kGoogleLoggingService,
                              kGoogleWriteLogEntriesMethod, *req,
                              kDefaultTimeoutMillisecond, std::move(handler));
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
