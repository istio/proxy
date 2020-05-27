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

#pragma once

#include "extensions/stackdriver/common/metrics.h"
#include "extensions/stackdriver/common/utils.h"
#include "extensions/stackdriver/edges/edges.pb.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "include/proxy-wasm/null_plugin.h"
namespace proxy_wasm {
namespace null_plugin {

#endif

namespace Extensions {
namespace Stackdriver {
namespace Edges {

using google::cloud::meshtelemetry::v1alpha1::ReportTrafficAssertionsRequest;

// MeshEdgesServiceClient provides a client interface for comms with an edges
// service (defined in edges.proto).
class MeshEdgesServiceClient {
 public:
  virtual ~MeshEdgesServiceClient() {}

  // reportTrafficAssertions handles invoking the `ReportTrafficAssertions` rpc.
  virtual void reportTrafficAssertions(
      const ReportTrafficAssertionsRequest& request) const = 0;
};

// MeshEdgesServiceClientImpl provides a gRPC implementation of the client
// interface. By default, it will write the meshtelemetry backend provided
// by Stackdriver, using application default credentials.
class MeshEdgesServiceClientImpl : public MeshEdgesServiceClient {
 public:
  // root_context is the wasm runtime context
  // edges_endpoint is an optional param used to specify alternative service
  // address.
  MeshEdgesServiceClientImpl(
      RootContext* root_context,
      const ::Extensions::Stackdriver::Common::StackdriverStubOption&
          stub_option);

  void reportTrafficAssertions(
      const ReportTrafficAssertionsRequest& request) const override;

 private:
  // Provides the VM context for making calls.
  RootContext* context_ = nullptr;

  // edges service endpoint.
  std::string grpc_service_;

  // callbacks for the client
  std::function<void(size_t)> success_callback_;
  std::function<void(GrpcStatus)> failure_callback_;
};

}  // namespace Edges
}  // namespace Stackdriver
}  // namespace Extensions

#ifdef NULL_PLUGIN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
