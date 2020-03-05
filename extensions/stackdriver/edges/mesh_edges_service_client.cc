
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

#include "extensions/stackdriver/edges/mesh_edges_service_client.h"

#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/common/utils.h"
#include "google/protobuf/util/time_util.h"

#ifdef NULL_PLUGIN
namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

namespace {

// StackdriverContextGraphHandler is used to inject x-goog-user-project header
// into gRPC initial metadata. This is a work around since gRPC library is not
// yet able to add that header automatically based on quota project from STS
// token. This should not be needed after
// https://github.com/grpc/grpc/issues/21225 is ready.
class StackdriverContextGraphHandler
    : public GrpcCallHandler<google::protobuf::Empty> {
 public:
  StackdriverContextGraphHandler(const std::string& project_id)
      : GrpcCallHandler(), project_id_(project_id) {}

  void onCreateInitialMetadata() override {
    addHeaderMapValue(
        HeaderMapType::GrpcCreateInitialMetadata,
        ::Extensions::Stackdriver::Common::kGoogleUserProjectHeaderKey,
        project_id_);
  }

  void onSuccess(google::protobuf::Empty&&) override {
    // TODO(douglas-reid): improve logging message.
    logDebug(
        "successfully sent MeshEdgesService ReportTrafficAssertionsRequest");
  }

  void onFailure(GrpcStatus status,
                 std::unique_ptr<WasmData> message) override {
    // TODO(douglas-reid): add retry (and other) logic
    logWarn("MeshEdgesService ReportTrafficAssertionsRequest failure: " +
            std::to_string(static_cast<int>(status)) + " " +
            message->toString());
  }

 private:
  const std::string& project_id_;
};

}  // namespace

using envoy::api::v2::core::GrpcService;
using Envoy::Extensions::Common::Wasm::Null::Plugin::GrpcStatus;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logDebug;
using Envoy::Extensions::Common::Wasm::Null::Plugin::logWarn;
using Envoy::Extensions::Common::Wasm::Null::Plugin::StringView;
#endif

// TODO(douglas-reid): confirm values here
constexpr char kMeshTelemetryService[] = "meshtelemetry.googleapis.com";
constexpr char kMeshEdgesService[] =
    "google.cloud.meshtelemetry.v1alpha1.MeshEdgesService";
constexpr char kReportTrafficAssertions[] = "ReportTrafficAssertions";
constexpr char kDefaultRootCertFile[] = "/etc/ssl/certs/ca-certificates.crt";
constexpr int kDefaultTimeoutMillisecond = 10000;  // 10 seconds

namespace Extensions {
namespace Stackdriver {
namespace Edges {

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;
using google::protobuf::util::TimeUtil;

MeshEdgesServiceClientImpl::MeshEdgesServiceClientImpl(
    RootContext* root_context, const std::string& edges_endpoint,
    const std::string& project_id, const std::string& sts_port)
    : context_(root_context), project_id_(project_id) {
  GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("mesh_edges");
  if (edges_endpoint.empty()) {
    // use application default creds and default target
    grpc_service.mutable_google_grpc()->set_target_uri(kMeshTelemetryService);
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
          sts_port);
    }
    grpc_service.mutable_google_grpc()
        ->mutable_channel_credentials()
        ->mutable_ssl_credentials()
        ->mutable_root_certs()
        ->set_filename(kDefaultRootCertFile);
  } else {
    // no creds attached
    grpc_service.mutable_google_grpc()->set_target_uri(edges_endpoint);
  }

  grpc_service.SerializeToString(&grpc_service_);
};

void MeshEdgesServiceClientImpl::reportTrafficAssertions(
    const ReportTrafficAssertionsRequest& request) const {
  auto handler = std::make_unique<StackdriverContextGraphHandler>(project_id_);
  context_->grpcCallHandler(grpc_service_, kMeshEdgesService,
                            kReportTrafficAssertions, request,
                            kDefaultTimeoutMillisecond, std::move(handler));
};

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions

#ifdef NULL_PLUGIN
}  // namespace plugin
}  // namespace null
}  // namespace wasm
}  // namespace common
}  // namespace extensions
}  // namespace envoy
#endif
