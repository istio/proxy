#include "datadog_agent_config.h"

#include <algorithm>
#include <chrono>
#include <cstddef>

#include "default_http_client.h"
#include "environment.h"
#include "parse_util.h"
#include "threaded_event_scheduler.h"

namespace datadog {
namespace tracing {

Expected<DatadogAgentConfig> load_datadog_agent_env_config() {
  DatadogAgentConfig env_config;

  if (auto rc_enabled = lookup(environment::DD_REMOTE_CONFIGURATION_ENABLED)) {
    env_config.remote_configuration_enabled = !falsy(*rc_enabled);
  }

  if (auto raw_rc_poll_interval_value =
          lookup(environment::DD_REMOTE_CONFIG_POLL_INTERVAL_SECONDS)) {
    auto res = parse_double(*raw_rc_poll_interval_value);
    if (auto error = res.if_error()) {
      return error->with_prefix(
          "DatadogAgent: Remote Configuration poll interval error ");
    }

    env_config.remote_configuration_poll_interval_seconds = *res;
  }

  auto env_host = lookup(environment::DD_AGENT_HOST);
  auto env_port = lookup(environment::DD_TRACE_AGENT_PORT);

  if (auto url_env = lookup(environment::DD_TRACE_AGENT_URL)) {
    env_config.url = std::string{*url_env};
  } else if (env_host || env_port) {
    std::string configured_url = "http://";
    append(configured_url, env_host.value_or("localhost"));
    configured_url += ':';
    append(configured_url, env_port.value_or("8126"));

    env_config.url = std::move(configured_url);
  }

  return env_config;
}

Expected<FinalizedDatadogAgentConfig> finalize_config(
    const DatadogAgentConfig& user_config,
    const std::shared_ptr<Logger>& logger, const Clock& clock) {
  Expected<DatadogAgentConfig> env_config = load_datadog_agent_env_config();
  if (auto error = env_config.if_error()) {
    return *error;
  }

  FinalizedDatadogAgentConfig result;

  result.clock = clock;

  if (!user_config.http_client) {
    result.http_client = default_http_client(logger, clock);
    // `default_http_client` might return a `Curl` instance depending on how
    // this library was built.  If it returns `nullptr`, then there's no
    // built-in default, and so the user must provide a value.
    if (!result.http_client) {
      return Error{Error::DATADOG_AGENT_NULL_HTTP_CLIENT,
                   "DatadogAgent: HTTP client cannot be null."};
    }
  } else {
    result.http_client = user_config.http_client;
  }

  if (!user_config.event_scheduler) {
    result.event_scheduler = std::make_shared<ThreadedEventScheduler>();
  } else {
    result.event_scheduler = user_config.event_scheduler;
  }

  if (auto flush_interval_milliseconds =
          value_or(env_config->flush_interval_milliseconds,
                   user_config.flush_interval_milliseconds, 2000);
      flush_interval_milliseconds > 0) {
    result.flush_interval =
        std::chrono::milliseconds(flush_interval_milliseconds);
  } else {
    return Error{Error::DATADOG_AGENT_INVALID_FLUSH_INTERVAL,
                 "DatadogAgent: Flush interval must be a positive number of "
                 "milliseconds."};
  }

  if (auto request_timeout_milliseconds =
          value_or(env_config->request_timeout_milliseconds,
                   user_config.request_timeout_milliseconds, 2000);
      request_timeout_milliseconds > 0) {
    result.request_timeout =
        std::chrono::milliseconds(request_timeout_milliseconds);
  } else {
    return Error{Error::DATADOG_AGENT_INVALID_REQUEST_TIMEOUT,
                 "DatadogAgent: Request timeout must be a positive number of "
                 "milliseconds."};
  }

  if (auto shutdown_timeout_milliseconds =
          value_or(env_config->shutdown_timeout_milliseconds,
                   user_config.shutdown_timeout_milliseconds, 2000);
      shutdown_timeout_milliseconds > 0) {
    result.shutdown_timeout =
        std::chrono::milliseconds(shutdown_timeout_milliseconds);
  } else {
    return Error{Error::DATADOG_AGENT_INVALID_SHUTDOWN_TIMEOUT,
                 "DatadogAgent: Shutdown timeout must be a positive number of "
                 "milliseconds."};
  }

  if (double rc_poll_interval_seconds =
          value_or(env_config->remote_configuration_poll_interval_seconds,
                   user_config.remote_configuration_poll_interval_seconds, 5.0);
      rc_poll_interval_seconds >= 0.0) {
    result.remote_configuration_poll_interval =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(rc_poll_interval_seconds));
  } else {
    return Error{Error::DATADOG_AGENT_INVALID_REMOTE_CONFIG_POLL_INTERVAL,
                 "DatadogAgent: Remote Configuration poll interval must be a "
                 "positive number of seconds."};
  }

  result.remote_configuration_enabled =
      value_or(env_config->remote_configuration_enabled,
               user_config.remote_configuration_enabled, true);

  const auto [origin, url] =
      pick(env_config->url, user_config.url, "http://localhost:8126");
  auto parsed_url = HTTPClient::URL::parse(url);
  if (auto* error = parsed_url.if_error()) {
    return std::move(*error);
  }
  result.url = *parsed_url;
  result.metadata[ConfigName::AGENT_URL] =
      ConfigMetadata(ConfigName::AGENT_URL, url, origin);

  return result;
}

}  // namespace tracing
}  // namespace datadog
