#pragma once

#include <datadog/clock.h>
#include <datadog/config.h>
#include <datadog/event_scheduler.h>
#include <datadog/http_client.h>
#include <datadog/logger.h>
#include <datadog/telemetry/configuration.h>
#include <datadog/telemetry/metrics.h>
#include <datadog/tracer_signature.h>

#include <memory>
#include <vector>

/// Telemetry functions are responsibles for handling internal telemetry data to
/// track Datadog product usage. It _can_ collect and report logs and metrics.
///
/// IMPORTANT: This is intended for use only by Datadog Engineers.
namespace datadog::telemetry {

/// Initialize the telemetry module
/// Once initialized, sends a notification indicating that the application has
/// started. The telemetry module is running for the entire lifecycle of the
/// application.
///
/// @param configuration The finalized configuration settings.
/// @param logger User logger instance.
/// @param metrics A vector user metrics to report.
///
/// NOTE: Make sure to call `init` before calling any of the other telemetry
/// functions.
void init(FinalizedConfiguration configuration,
          std::shared_ptr<tracing::Logger> logger,
          std::shared_ptr<tracing::HTTPClient> client,
          std::shared_ptr<tracing::EventScheduler> event_scheduler,
          tracing::HTTPClient::URL agent_url,
          tracing::Clock clock = tracing::default_clock);

void init(FinalizedConfiguration configuration,
          tracing::TracerSignature tracer_signature,
          std::shared_ptr<tracing::Logger> logger,
          std::shared_ptr<tracing::HTTPClient> client,
          std::shared_ptr<tracing::EventScheduler> event_scheduler,
          tracing::HTTPClient::URL agent_url,
          tracing::Clock clock = tracing::default_clock);

/// Sends configuration changes.
///
/// This function is responsible for sending reported configuration changes
/// reported by `capture_configuration_change`.
///
/// @note This function should be called _AFTER_ all configuration changes are
/// captures by `capture_configuration_change`.
void send_configuration_change();

/// Captures a change in the application's configuration.
///
/// This function is called to report updates to the application's
/// configuration. It takes a vector of new configuration metadata as a
/// parameter, which contains the updated settings.
///
/// @param new_configuration A vector containing the new configuration metadata.
///
/// @note This function should be invoked whenever there is a change in the
/// configuration.
void capture_configuration_change(
    const std::vector<tracing::ConfigMetadata>& new_configuration);

/// The `log` namespace provides functions for reporting logs.
namespace log {
/// Report internal warning message to Datadog.
///
/// @param message The warning message to log.
void warning(std::string message);

/// Report internal error message to Datadog.
///
/// @param message The error message.
void error(std::string message);

/// Report internal error message to Datadog.
///
/// @param message The error message.
/// @param stacktrace Stacktrace leading to the error.
void error(std::string message, std::string stacktrace);
}  // namespace log

/// The `counter` namespace provides functions to track values.
/// Counters can be useful for tracking the total number of an event occurring
/// in one time interval. For example, the amount of requests, errors or jobs
/// processed every 10 seconds.
namespace counter {

/// Increments the specified counter by 1.
///
/// @param `counter` the counter to increment.
void increment(const Counter& counter);

/// Increments the specified counter by 1.
///
/// @param `counter` the counter to increment.
/// @param `tags` the distribution tags.
void increment(const Counter& counter, const std::vector<std::string>& tags);

/// Decrements the specified counter by 1.
///
/// @param `counter` the counter to decrement.
void decrement(const Counter& counter);

/// Decrements the specified counter by 1.
///
/// @param `counter` the counter to decrement.
/// @param `tags` the distribution tags.
void decrement(const Counter& counter, const std::vector<std::string>& tags);

/// Sets the counter to a specific value.
///
/// @param `counter` the counter to update.
/// @param `value` the value to assign to the counter.
void set(const Counter& counter, uint64_t value);

/// Sets the counter to a specific value.
///
/// @param `counter` the counter to update.
/// @param `tags` the distribution tags.
/// @param `value` the value to assign to the counter.
void set(const Counter& counter, const std::vector<std::string>& tags,
         uint64_t value);

}  // namespace counter

/// The `rate` namespace provides support for rate metrics-values.
/// Rates can be useful for tracking the total number of an event occurrences in
/// one time interval. For example, the number of requests per second.
namespace rate {

/// Sets the rate to a specific value.
///
/// @param `rate` the rate to update.
/// @param `value` the value to assign to the counter.
void set(const Rate& rate, uint64_t value);

/// Sets the rate to a specific value.
///
/// @param `rate` the rate to update.
/// @param `tags` the distribution tags.
/// @param `value` the value to assign to the counter.
void set(const Rate& rate, const std::vector<std::string>&, uint64_t value);

}  // namespace rate

/// The `distribution` namespace provides support for statistical distribution.
/// Distribution can be useful for tracking things like response times or
/// payload sizes.
namespace distribution {

/// Adds a value to the distribution.
///
/// @param `distribution` the distribution to update.
/// @param `value` the value to add to the distribution.
void add(const Distribution& distribution, uint64_t value);

/// Adds a value to the distribution.
///
/// @param `distribution` the distribution to update.
/// @param `tags` the distribution tags.
/// @param `value` the value to add to the distribution.
void add(const Distribution& distribution, const std::vector<std::string>& tags,
         uint64_t value);

}  // namespace distribution

}  // namespace datadog::telemetry
