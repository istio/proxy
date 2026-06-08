#include <datadog/config.h>
#include <datadog/environment.h>
#include <datadog/telemetry/configuration.h>
#include <datadog/version.h>

#include "parse_util.h"

using namespace datadog::tracing;

namespace datadog::telemetry {

namespace {

tracing::Expected<Configuration> load_telemetry_env_config() {
  Configuration env_cfg;

  if (auto enabled_env =
          lookup(environment::DD_INSTRUMENTATION_TELEMETRY_ENABLED)) {
    env_cfg.enabled = !falsy(*enabled_env);
  }

  if (auto metrics_enabled =
          lookup(environment::DD_TELEMETRY_METRICS_ENABLED)) {
    env_cfg.report_metrics = !falsy(*metrics_enabled);
  }

  if (auto logs_enabled =
          lookup(environment::DD_TELEMETRY_LOG_COLLECTION_ENABLED)) {
    env_cfg.report_logs = !falsy(*logs_enabled);
  }

  if (auto metrics_interval_seconds =
          lookup(environment::DD_TELEMETRY_METRICS_INTERVAL_SECONDS)) {
    auto maybe_value = parse_double(*metrics_interval_seconds);
    if (auto error = maybe_value.if_error()) {
      return *error;
    }
    env_cfg.metrics_interval_seconds = *maybe_value;
  }

  if (auto heartbeat_interval_seconds =
          lookup(environment::DD_TELEMETRY_HEARTBEAT_INTERVAL)) {
    auto maybe_value = parse_double(*heartbeat_interval_seconds);
    if (auto error = maybe_value.if_error()) {
      return *error;
    }

    env_cfg.heartbeat_interval_seconds = *maybe_value;
  }

  return env_cfg;
}

}  // namespace

tracing::Expected<FinalizedConfiguration> finalize_config(
    const Configuration& user_config) {
  auto env_config = load_telemetry_env_config();
  if (auto error = env_config.if_error()) {
    return *error;
  }

  ConfigMetadata::Origin origin;
  FinalizedConfiguration result;

  // enabled
  std::tie(origin, result.enabled) =
      pick(env_config->enabled, user_config.enabled, true);

  if (!result.enabled) {
    // NOTE(@dmehala): if the telemetry module is disabled then report metrics
    // is also disabled.
    result.report_metrics = false;
    result.report_logs = false;
  } else {
    // report_metrics
    std::tie(origin, result.report_metrics) =
        pick(env_config->report_metrics, user_config.report_metrics, true);

    // report_logs
    std::tie(origin, result.report_logs) =
        pick(env_config->report_logs, user_config.report_logs, true);
  }

  // debug
  if (auto enabled_debug_env = lookup(environment::DD_TELEMETRY_DEBUG)) {
    result.debug = !falsy(*enabled_debug_env);
  } else {
    result.debug = false;
  }

  // metrics_interval_seconds
  auto metrics_interval = pick(env_config->metrics_interval_seconds,
                               user_config.metrics_interval_seconds, 60);
  if (metrics_interval.second <= 0.) {
    return Error{Error::Code::OUT_OF_RANGE_INTEGER,
                 "Telemetry metrics polling interval must be a positive value"};
  }
  result.metrics_interval =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double>(metrics_interval.second));

  // heartbeat_interval_seconds
  auto heartbeat_interval = pick(env_config->heartbeat_interval_seconds,
                                 user_config.heartbeat_interval_seconds, 10);
  if (heartbeat_interval.second <= 0.) {
    return Error{
        Error::Code::OUT_OF_RANGE_INTEGER,
        "Telemetry heartbeat polling interval must be a positive value"};
  }
  result.heartbeat_interval =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double>(heartbeat_interval.second));

  // integration_name
  std::tie(origin, result.integration_name) =
      pick(env_config->integration_name, user_config.integration_name,
           std::string("datadog"));

  // integration_version
  std::tie(origin, result.integration_version) =
      pick(env_config->integration_version, user_config.integration_version,
           tracing::tracer_version);

  // products
  result.products = user_config.products;

  // onboarding data
  if (auto install_id = lookup(environment::DD_INSTRUMENTATION_INSTALL_ID)) {
    result.install_id = std::string(*install_id);
  }
  if (auto install_type =
          lookup(environment::DD_INSTRUMENTATION_INSTALL_TYPE)) {
    result.install_type = std::string(*install_type);
  }
  if (auto install_time =
          lookup(environment::DD_INSTRUMENTATION_INSTALL_TIME)) {
    result.install_time = std::string(*install_time);
  }

  return result;
}

}  // namespace datadog::telemetry
