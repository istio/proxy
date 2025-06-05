#pragma once

// This component provides facilities for configuring a `DatadogAgent`.
//
// `struct DatadogAgentConfig` contains fields that are used to configure
// `DatadogAgent`.  The configuration must first be finalized before it can be
// used by `DatadogAgent`.  The function `finalize_config` produces either an
// error or a `FinalizedDatadogAgentConfig`.  The latter can be used by
// `DatadogAgent`.
//
// Typical usage of `DatadogAgentConfig` is implicit as part of `TracerConfig`.
// See `tracer_config.h`.

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

#include "clock.h"
#include "config.h"
#include "expected.h"
#include "http_client.h"
#include "string_view.h"

namespace datadog {
namespace tracing {

class EventScheduler;
class Logger;

struct DatadogAgentConfig {
  // The `HTTPClient` used to submit traces to the Datadog Agent.  If this
  // library was built with libcurl (the default), then `http_client` is
  // optional: a `Curl` instance will be used if `http_client` is left null.
  // If this library was built without libcurl, then `http_client` is required
  // not to be null.
  std::shared_ptr<HTTPClient> http_client = nullptr;
  // The `EventScheduler` used to periodically submit batches of traces to the
  // Datadog Agent.  If `event_scheduler` is null, then a
  // `ThreadedEventScheduler` instance will be used instead.
  std::shared_ptr<EventScheduler> event_scheduler = nullptr;
  // A URL at which the Datadog Agent can be contacted.
  // The following formats are supported:
  //
  // - http://<domain or IP>:<port>
  // - http://<domain or IP>
  // - http+unix://<path to socket>
  // - unix://<path to socket>
  //
  // The port defaults to 8126 if it is not specified.
  Optional<std::string> url;
  // How often, in milliseconds, to send batches of traces to the Datadog Agent.
  Optional<int> flush_interval_milliseconds;
  // Maximum amount of time an HTTP request is allowed to run.
  Optional<int> request_timeout_milliseconds;
  // Maximum amount of time the process is allowed to wait before shutting down.
  Optional<int> shutdown_timeout_milliseconds;
  // Enable the capability that allows to remotely configure and change the
  // behavior of the tracer.
  Optional<bool> remote_configuration_enabled;
  // How often, in seconds, to query the Datadog Agent for remote configuration
  // updates.
  Optional<double> remote_configuration_poll_interval_seconds;

  static Expected<HTTPClient::URL> parse(StringView);
};

class FinalizedDatadogAgentConfig {
  friend Expected<FinalizedDatadogAgentConfig> finalize_config(
      const DatadogAgentConfig&, const std::shared_ptr<Logger>&, const Clock&);

  FinalizedDatadogAgentConfig() = default;

 public:
  Clock clock;
  bool remote_configuration_enabled;
  std::shared_ptr<HTTPClient> http_client;
  std::shared_ptr<EventScheduler> event_scheduler;
  HTTPClient::URL url;
  std::chrono::steady_clock::duration flush_interval;
  std::chrono::steady_clock::duration request_timeout;
  std::chrono::steady_clock::duration shutdown_timeout;
  std::chrono::steady_clock::duration remote_configuration_poll_interval;
  std::unordered_map<ConfigName, ConfigMetadata> metadata;
};

Expected<FinalizedDatadogAgentConfig> finalize_config(
    const DatadogAgentConfig& config, const std::shared_ptr<Logger>& logger,
    const Clock& clock);

}  // namespace tracing
}  // namespace datadog
