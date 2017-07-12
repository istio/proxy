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

#pragma once

#include <memory>

#include "common/common/logger.h"
#include "envoy/event/dispatcher.h"
#include "envoy/grpc/rpc_channel.h"

#include "envoy/upstream/cluster_manager.h"
#include "include/client.h"

namespace Envoy {
namespace Http {
namespace Mixer {

// A helper class to pass Upstream::ClusterManager& in the lambda capture
// If passing Upstream::ClusterManager& directly in the lambda capture,
// it assumes const Upstream::ClusterManager&.
class GrpcTransportInitData {
 public:
  GrpcTransportInitData(Upstream::ClusterManager& cm)
      : cm_(cm), headers_(nullptr) {}
  GrpcTransportInitData(Upstream::ClusterManager& cm, const HeaderMap* headers)
      : cm_(cm), headers_(headers) {}

  Upstream::ClusterManager& cm() const { return cm_; }
  const HeaderMap* headers() const { return headers_; }

 private:
  Upstream::ClusterManager& cm_;
  const HeaderMap* headers_;
};

// An object to use Envoy async_client to make grpc call.
class GrpcTransport : public Grpc::RpcChannelCallbacks,
                      public Logger::Loggable<Logger::Id::http> {
 public:
  GrpcTransport(const GrpcTransportInitData& cm);

  void onPreRequestCustomizeHeaders(Http::HeaderMap& headers) override;

  void onSuccess() override;

  void onFailure(const Optional<uint64_t>& grpc_status,
                 const std::string& message) override;

  // Check if mixer server cluster configured in cluster_manager.
  static bool IsMixerServerConfigured(Upstream::ClusterManager& cm);

 protected:
  // Create a new grpc channel.
  Grpc::RpcChannelPtr NewChannel(Upstream::ClusterManager& cm);

  // The on_done callback function.
  ::istio::mixer_client::DoneFunc on_done_;
  // the grpc channel.
  Grpc::RpcChannelPtr channel_;
  // The generated mixer client stub.
  ::istio::mixer::v1::Mixer::Stub stub_;
  // The header map from the origin client request.
  const HeaderMap* headers_;
};

class CheckGrpcTransport : public GrpcTransport {
 public:
  CheckGrpcTransport(const GrpcTransportInitData& cm) : GrpcTransport(cm) {}

  static ::istio::mixer_client::TransportCheckFunc GetFunc(
      std::shared_ptr<GrpcTransportInitData> cms) {
    return [cms](const ::istio::mixer::v1::CheckRequest& request,
                 ::istio::mixer::v1::CheckResponse* response,
                 ::istio::mixer_client::DoneFunc on_done) {
      CheckGrpcTransport* transport = new CheckGrpcTransport(*cms);
      transport->Call(request, response, on_done);
    };
  }
  void Call(const ::istio::mixer::v1::CheckRequest& request,
            ::istio::mixer::v1::CheckResponse* response,
            ::istio::mixer_client::DoneFunc on_done) {
    on_done_ = [this, response,
                on_done](const ::google::protobuf::util::Status& status) {
      if (status.ok()) {
        log().debug("Check response: {}", response->DebugString());
      }
      on_done(status);
    };
    log().debug("Call grpc check: {}", request.DebugString());
    stub_.Check(nullptr, &request, response, nullptr);
  }
};

class ReportGrpcTransport : public GrpcTransport {
 public:
  ReportGrpcTransport(const GrpcTransportInitData& cm) : GrpcTransport(cm) {}

  static ::istio::mixer_client::TransportReportFunc GetFunc(
      std::shared_ptr<GrpcTransportInitData> cms) {
    return [cms](const ::istio::mixer::v1::ReportRequest& request,
                 ::istio::mixer::v1::ReportResponse* response,
                 ::istio::mixer_client::DoneFunc on_done) {
      ReportGrpcTransport* transport = new ReportGrpcTransport(*cms);
      transport->Call(request, response, on_done);
    };
  }
  void Call(const ::istio::mixer::v1::ReportRequest& request,
            ::istio::mixer::v1::ReportResponse* response,
            ::istio::mixer_client::DoneFunc on_done) {
    on_done_ = on_done;
    log().debug("Call grpc report: {}", request.DebugString());
    stub_.Report(nullptr, &request, response, nullptr);
  }
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
