#pragma once

#include <datadog/clock.h>
#include <datadog/config.h>
#include <datadog/event_scheduler.h>
#include <datadog/http_client.h>
#include <datadog/logger.h>
#include <datadog/telemetry/configuration.h>
#include <datadog/telemetry/metrics.h>
#include <datadog/tracer_signature.h>

#include <mutex>

#include "json.hpp"
#include "log.h"
#include "metric_context.h"
#include "platform_util.h"

namespace datadog::telemetry {

using MetricSnapshot = std::vector<std::pair<std::time_t, uint64_t>>;

/// The telemetry class is responsible for handling internal telemetry data to
/// track Datadog product usage. It _can_ collect and report logs and metrics.
///
/// NOTE(@dmehala): The current implementation can lead a significant amount
/// of overhead if the mutext is highly disputed. Unless this is proven to be
/// indeed a bottleneck, I'll embrace KISS principle. However, in a future
/// iteration we could use multiple producer single consumer queue or
/// lock-free queue.
class Telemetry final {
  /// Configuration object containing the validated settings for telemetry
  FinalizedConfiguration config_;
  /// Shared pointer to the user logger instance.
  std::shared_ptr<tracing::Logger> logger_;
  std::vector<tracing::EventScheduler::Cancel> tasks_;
  tracing::HTTPClient::URL telemetry_endpoint_;
  tracing::TracerSignature tracer_signature_;
  std::shared_ptr<tracing::HTTPClient> http_client_;
  tracing::Clock clock_;
  std::shared_ptr<tracing::EventScheduler> scheduler_;

  /// Counter
  std::mutex counter_mutex_;
  std::unordered_map<MetricContext<Counter>, uint64_t> counters_;
  std::unordered_map<MetricContext<Counter>, MetricSnapshot> counters_snapshot_;

  /// Rate
  std::mutex rate_mutex_;
  std::unordered_map<MetricContext<Rate>, uint64_t> rates_;
  std::unordered_map<MetricContext<Rate>, MetricSnapshot> rates_snapshot_;

  /// Distribution
  /// TODO: split distribution in array of N element?
  std::mutex distributions_mutex_;
  std::unordered_map<MetricContext<Distribution>, std::vector<uint64_t>>
      distributions_;

  /// Configuration
  std::vector<tracing::ConfigMetadata> configuration_snapshot_;

  std::mutex log_mutex_;
  std::vector<telemetry::LogMessage> logs_;

  // Track sequence id per payload generated
  uint64_t seq_id_ = 0;
  // Track sequence id per configuration field
  std::unordered_map<tracing::ConfigName, std::size_t> config_seq_ids_;

  tracing::HostInfo host_info_;

 public:
  /// Constructor for the Telemetry class
  ///
  /// @param configuration The finalized configuration settings.
  /// @param logger User logger instance.
  /// @param metrics A vector user metrics to report.
  Telemetry(FinalizedConfiguration configuration,
            tracing::TracerSignature tracer_signature,
            std::shared_ptr<tracing::Logger> logger,
            std::shared_ptr<tracing::HTTPClient> client,
            std::shared_ptr<tracing::EventScheduler> event_scheduler,
            tracing::HTTPClient::URL agent_url,
            tracing::Clock clock = tracing::default_clock);

  /// Destructor
  ///
  /// Send last metrics snapshot and `app-closing` event.
  ~Telemetry();

  /// Move semantics.
  Telemetry(Telemetry&& rhs);
  Telemetry& operator=(Telemetry&&);

  /// Capture and report internal error message to Datadog.
  ///
  /// @param message The error message.
  void log_error(std::string message);
  void log_error(std::string message, std::string stacktrace);

  /// capture and report internal warning message to Datadog.
  ///
  /// @param message The warning message to log.
  void log_warning(std::string message);

  void send_configuration_change();

  void capture_configuration_change(
      const std::vector<tracing::ConfigMetadata>& new_configuration);

  /// Counter
  void increment_counter(const Counter& counter);
  void increment_counter(const Counter& counter,
                         const std::vector<std::string>& tags);
  void decrement_counter(const Counter& counter);
  void decrement_counter(const Counter& counter,
                         const std::vector<std::string>& tags);
  void set_counter(const Counter& counter, uint64_t value);
  void set_counter(const Counter& counter, const std::vector<std::string>& tags,
                   uint64_t value);

  /// Rate
  void set_rate(const Rate& rate, uint64_t value);
  void set_rate(const Rate& rate, const std::vector<std::string>& tags,
                uint64_t value);

  /// Distribution
  void add_datapoint(const Distribution& distribution, uint64_t value);
  void add_datapoint(const Distribution& distribution,
                     const std::vector<std::string>& tags, uint64_t value);

 private:
  void app_started();
  void app_closing();

  void send_payload(tracing::StringView request_type, std::string payload);

  void schedule_tasks();

  void capture_metrics();

  void log(std::string message, telemetry::LogLevel level,
           tracing::Optional<std::string> stacktrace = tracing::nullopt);

  nlohmann::json generate_telemetry_body(std::string request_type);
  nlohmann::json generate_configuration_field(
      const tracing::ConfigMetadata& config_metadata);

  // Constructs an `app-started` message using information provided when
  // constructed and the tracer_config value passed in.
  std::string app_started_payload();
  // Constructs a messsage-batch containing `app-heartbeat`, and if metrics
  // have been modified, a `generate-metrics` message.
  std::string heartbeat_and_telemetry();
  // Constructs a message-batch containing `app-closing`, and if metrics have
  // been modified, a `generate-metrics` message.
  std::string app_closing_payload();
};

}  // namespace datadog::telemetry
