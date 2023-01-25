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

#include <string>

#include "extensions/stackdriver/common/utils.h"
#include "google/logging/v2/logging.pb.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
#endif

namespace Extensions {
namespace Stackdriver {
namespace Log {

// Log exporter interface.
class Exporter {
public:
  virtual ~Exporter() {}

  virtual void
  exportLogs(const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>&,
             bool is_on_done) = 0;
};

// Exporter writes Stackdriver access log to the backend. It uses WebAssembly
// gRPC API.
class ExporterImpl : public Exporter {
public:
  // root_context is the wasm runtime context that this instance runs with.
  // logging_service_endpoint is an optional param which should be used for test
  // only.
  ExporterImpl(RootContext* root_context,
               const ::Extensions::Stackdriver::Common::StackdriverStubOption& stub_option);

  // exportLogs exports the given log request to Stackdriver.
  void exportLogs(
      const std::vector<std::unique_ptr<const google::logging::v2::WriteLogEntriesRequest>>& req,
      bool is_on_done) override;

private:
  // Wasm context that outbound calls are attached to.
  RootContext* context_ = nullptr;

  // Serialized string of Stackdriver logging service
  std::string grpc_service_string_;

  // Indicates if the current exporting is triggered by root context onDone. If
  // this is true, gRPC callback needs to call proxy_done to indicate that async
  // call finishes.
  bool is_on_done_ = false;

  // Callbacks for gRPC calls.
  std::function<void(size_t)> success_callback_;
  std::function<void(GrpcStatus)> failure_callback_;

  // Record in flight export calls. When ondone is triggered, export call needs
  // to be zero before calling proxy_done.
  int in_flight_export_call_ = 0;
};

} // namespace Log
} // namespace Stackdriver
} // namespace Extensions

#ifdef NULL_PLUGIN
} // namespace null_plugin
} // namespace proxy_wasm
#endif
