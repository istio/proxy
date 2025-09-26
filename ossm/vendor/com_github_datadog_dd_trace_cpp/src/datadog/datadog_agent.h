#pragma once

// This component provides a `class`, `DatadogAgent`, that implements the
// `Collector` interface in terms of periodic HTTP requests to a Datadog Agent.
//
// `DatadogAgent` is configured by `DatadogAgentConfig`.  See
// `datadog_agent_config.h`.

#include <memory>
#include <mutex>
#include <vector>

#include "clock.h"
#include "collector.h"
#include "config_manager.h"
#include "event_scheduler.h"
#include "http_client.h"
#include "metrics.h"
#include "remote_config.h"
#include "tracer_signature.h"
#include "tracer_telemetry.h"

namespace datadog {
namespace tracing {

class FinalizedDatadogAgentConfig;
class Logger;
struct SpanData;
class TraceSampler;
struct TracerSignature;

class DatadogAgent : public Collector {
 public:
  struct TraceChunk {
    std::vector<std::unique_ptr<SpanData>> spans;
    std::shared_ptr<TraceSampler> response_handler;
  };

 private:
  std::mutex mutex_;
  std::shared_ptr<TracerTelemetry> tracer_telemetry_;
  Clock clock_;
  std::shared_ptr<Logger> logger_;
  std::vector<TraceChunk> trace_chunks_;
  HTTPClient::URL traces_endpoint_;
  HTTPClient::URL telemetry_endpoint_;
  HTTPClient::URL remote_configuration_endpoint_;
  std::shared_ptr<HTTPClient> http_client_;
  std::shared_ptr<EventScheduler> event_scheduler_;
  std::vector<EventScheduler::Cancel> tasks_;
  std::chrono::steady_clock::duration flush_interval_;
  // Callbacks for submitting telemetry data
  HTTPClient::ResponseHandler telemetry_on_response_;
  HTTPClient::ErrorHandler telemetry_on_error_;
  std::chrono::steady_clock::duration request_timeout_;
  std::chrono::steady_clock::duration shutdown_timeout_;

  RemoteConfigurationManager remote_config_;
  TracerSignature tracer_signature_;

  void flush();
  void send_telemetry(StringView, std::string);
  void send_heartbeat_and_telemetry();
  void send_app_closing();

 public:
  DatadogAgent(const FinalizedDatadogAgentConfig&,
               const std::shared_ptr<TracerTelemetry>&,
               const std::shared_ptr<Logger>&, const TracerSignature& id,
               const std::shared_ptr<ConfigManager>& config_manager);
  ~DatadogAgent();

  Expected<void> send(
      std::vector<std::unique_ptr<SpanData>>&& spans,
      const std::shared_ptr<TraceSampler>& response_handler) override;

  void send_app_started(
      const std::unordered_map<ConfigName, ConfigMetadata>& config_metadata);

  void send_configuration_change(const std::vector<ConfigMetadata>& config);

  void get_and_apply_remote_configuration_updates();

  nlohmann::json config_json() const override;
};

}  // namespace tracing
}  // namespace datadog
