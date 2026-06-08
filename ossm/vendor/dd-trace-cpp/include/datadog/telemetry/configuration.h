#pragma once

#include <datadog/config.h>
#include <datadog/expected.h>
#include <datadog/optional.h>
#include <datadog/telemetry/product.h>

#include <chrono>
#include <string>
#include <vector>

namespace datadog::telemetry {

struct Configuration {
  // Enable or disable the telemetry module.
  // Default: enabled.
  // Can be overriden by the `DD_INSTRUMENTATION_TELEMETRY_ENABLED` environment
  // variable.
  tracing::Optional<bool> enabled;
  // Enable or disable telemetry metrics.
  // Default: enabled.
  // Can be overriden by the `DD_TELEMETRY_METRICS_ENABLED` environment
  // variable.
  tracing::Optional<bool> report_metrics;
  // Interval at which the metrics payload will be sent.
  // Can be overriden by `DD_TELEMETRY_METRICS_INTERVAL_SECONDS` environment
  // variable.
  tracing::Optional<double> metrics_interval_seconds;
  // Interval at which the heartbeat payload will be sent.
  // Can be overriden by `DD_TELEMETRY_HEARTBEAT_INTERVAL` environment variable.
  tracing::Optional<double> heartbeat_interval_seconds;
  // `integration_name` is the name of the product integrating this library.
  // Example: "nginx", "envoy" or "istio".
  tracing::Optional<std::string> integration_name;
  // `integration_version` is the version of the product integrating this
  // library.
  // Example: "1.2.3", "6c44da20", "2020.02.13"
  tracing::Optional<std::string> integration_version;
  // Enable or disable telemetry logs collection.
  // Default: enabled.
  // Can be overriden by the `DD_TELEMETRY_LOG_COLLECTION_ENABLED` environment
  // variable.
  tracing::Optional<bool> report_logs;
  // List of products reported in the `app-started` message.
  std::vector<Product> products;
};

struct FinalizedConfiguration {
  bool debug;
  bool enabled;
  bool report_metrics;
  bool report_logs;
  std::chrono::steady_clock::duration metrics_interval;
  std::chrono::steady_clock::duration heartbeat_interval;
  std::string integration_name;
  std::string integration_version;
  std::vector<Product> products;

  // Onboarding metadata coming from `DD_INSTRUMENTATION_INSTALL_*` environment
  // variables.
  tracing::Optional<std::string> install_id;
  tracing::Optional<std::string> install_type;
  tracing::Optional<std::string> install_time;

  friend tracing::Expected<FinalizedConfiguration> finalize_config(
      const Configuration&);
};

tracing::Expected<FinalizedConfiguration> finalize_config(
    const Configuration& = Configuration{});

}  // namespace datadog::telemetry
