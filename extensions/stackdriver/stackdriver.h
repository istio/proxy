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

#include "extensions/common/context.h"
#include "extensions/stackdriver/common/constants.h"
#include "extensions/stackdriver/config/v1alpha1/stackdriver_plugin_config.pb.h"
#include "extensions/stackdriver/edges/edge_reporter.h"
#include "extensions/stackdriver/log/logger.h"
#include "extensions/stackdriver/metric/record.h"

// OpenCensus is full of unused parameters in metric_service.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "opencensus/exporters/stats/stackdriver/stackdriver_exporter.h"
#pragma GCC diagnostic pop

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else

#include "extensions/common/wasm/null/null_plugin.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif

namespace Stackdriver {

constexpr long int kDefaultEdgeNewReportDurationNanoseconds =
    60000000000;  // 1m
constexpr long int kDefaultEdgeEpochReportDurationNanoseconds =
    600000000000;  // 10m

#ifdef NULL_PLUGIN
NULL_PLUGIN_REGISTRY;
#endif

// StackdriverRootContext is the root context for all streams processed by the
// thread. It has the same lifetime as the worker thread and acts as target for
// interactions that outlives individual stream, e.g. timer, async calls.
class StackdriverRootContext : public RootContext {
 public:
  StackdriverRootContext(uint32_t id, StringView root_id)
      : RootContext(id, root_id) {
    ::Wasm::Common::extractEmptyNodeFlatBuffer(&empty_node_info_);
  }
  ~StackdriverRootContext() = default;

  bool onConfigure(size_t) override;
  bool configure(size_t);
  bool onStart(size_t) override;
  void onTick() override;
  bool onDone() override;

  // Get direction of traffic relative to this proxy.
  bool isOutbound();

  bool useHostHeaderFallback() const { return use_host_header_fallback_; };

  // Records telemetry for the current active stream.
  void record();

  bool initialized() const { return initialized_; };

 private:
  // Indicates whether to export server access log or not.
  bool enableServerAccessLog();

  bool shouldLogThisRequest();

  // Indicates whether or not to report edges to Stackdriver.
  bool enableEdgeReporting();

  // Config for Stackdriver plugin.
  stackdriver::config::v1alpha1::PluginConfig config_;

  // Local node info extracted from node metadata.
  std::string local_node_info_;
  std::string empty_node_info_;

  // Indicates the traffic direction relative to this proxy.
  ::Wasm::Common::TrafficDirection direction_{
      ::Wasm::Common::TrafficDirection::Unspecified};

  // Logger records and exports log entries to Stackdriver backend.
  std::unique_ptr<::Extensions::Stackdriver::Log::Logger> logger_;

  std::unique_ptr<::Extensions::Stackdriver::Edges::EdgeReporter>
      edge_reporter_;

  long int last_edge_epoch_report_call_nanos_ = 0;

  long int last_edge_new_report_call_nanos_ = 0;

  long int edge_new_report_duration_nanos_ =
      kDefaultEdgeNewReportDurationNanoseconds;

  long int edge_epoch_report_duration_nanos_ =
      kDefaultEdgeEpochReportDurationNanoseconds;

  bool use_host_header_fallback_;
  bool initialized_ = false;
};

// StackdriverContext is per stream context. It has the same lifetime as
// the request stream itself.
class StackdriverContext : public Context {
 public:
  StackdriverContext(uint32_t id, RootContext* root) : Context(id, root) {}
  void onLog() override;

 private:
  // Gets root Stackdriver context that this stream Stackdriver context
  // associated with.
  StackdriverRootContext* getRootContext();
};

class StackdriverOutboundRootContext : public StackdriverRootContext {
 public:
  StackdriverOutboundRootContext(uint32_t id, StringView root_id)
      : StackdriverRootContext(id, root_id) {}
};

class StackdriverInboundRootContext : public StackdriverRootContext {
 public:
  StackdriverInboundRootContext(uint32_t id, StringView root_id)
      : StackdriverRootContext(id, root_id) {}
};

static RegisterContextFactory register_OutboundStackdriverContext(
    CONTEXT_FACTORY(StackdriverContext),
    ROOT_FACTORY(StackdriverOutboundRootContext),
    ::Extensions::Stackdriver::Common::kOutboundRootContextId);
static RegisterContextFactory register_InboundStackdriverContext(
    CONTEXT_FACTORY(StackdriverContext),
    ROOT_FACTORY(StackdriverInboundRootContext),
    ::Extensions::Stackdriver::Common::kInboundRootContextId);

}  // namespace Stackdriver

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
