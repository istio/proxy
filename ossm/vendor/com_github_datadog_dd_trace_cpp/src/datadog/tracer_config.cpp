#include "tracer_config.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cerr_logger.h"
#include "datadog_agent.h"
#include "environment.h"
#include "json.hpp"
#include "parse_util.h"
#include "string_util.h"
#include "string_view.h"

namespace datadog {
namespace tracing {
namespace {

Expected<std::vector<PropagationStyle>> parse_propagation_styles(
    StringView input) {
  std::vector<PropagationStyle> styles;

  const auto last_is_duplicate = [&]() -> Optional<Error> {
    assert(!styles.empty());

    const auto dupe =
        std::find(styles.begin(), styles.end() - 1, styles.back());
    if (dupe == styles.end() - 1) {
      return nullopt;  // no duplicate
    }

    std::string message;
    message += "The propagation style ";
    message += to_json(styles.back()).dump();
    message += " is duplicated in: ";
    append(message, input);
    return Error{Error::DUPLICATE_PROPAGATION_STYLE, std::move(message)};
  };

  // Style names are separated by spaces, or a comma, or some combination.
  for (const StringView &item : parse_list(input)) {
    if (const auto style = parse_propagation_style(item)) {
      styles.push_back(*style);
    } else {
      std::string message;
      message += "Unsupported propagation style \"";
      append(message, item);
      message += "\" in list \"";
      append(message, input);
      message +=
          "\".  The following styles are supported: Datadog, B3, tracecontext.";
      return Error{Error::UNKNOWN_PROPAGATION_STYLE, std::move(message)};
    }

    if (auto maybe_error = last_is_duplicate()) {
      return *maybe_error;
    }
  }

  return styles;
}

// Return a `std::vector<PropagationStyle>` parsed from the specified `env_var`.
// If `env_var` is not in the environment, return `nullopt`. If an error occurs,
// throw an `Error`.
Optional<std::vector<PropagationStyle>> styles_from_env(
    environment::Variable env_var) {
  const auto styles_env = lookup(env_var);
  if (!styles_env) {
    return {};
  }

  auto styles = parse_propagation_styles(*styles_env);
  if (auto *error = styles.if_error()) {
    std::string prefix;
    prefix += "Unable to parse ";
    append(prefix, name(env_var));
    prefix += " environment variable: ";
    throw error->with_prefix(prefix);
  }
  return *styles;
}

std::string json_quoted(StringView text) {
  std::string unquoted;
  assign(unquoted, text);
  return nlohmann::json(std::move(unquoted)).dump();
}

Expected<TracerConfig> load_tracer_env_config(Logger &logger) {
  TracerConfig env_cfg;

  if (auto service_env = lookup(environment::DD_SERVICE)) {
    env_cfg.service = std::string{*service_env};
  }

  if (auto environment_env = lookup(environment::DD_ENV)) {
    env_cfg.environment = std::string{*environment_env};
  }
  if (auto version_env = lookup(environment::DD_VERSION)) {
    env_cfg.version = std::string{*version_env};
  }

  if (auto tags_env = lookup(environment::DD_TAGS)) {
    auto tags = parse_tags(*tags_env);
    if (auto *error = tags.if_error()) {
      std::string prefix;
      prefix += "Unable to parse ";
      append(prefix, name(environment::DD_TAGS));
      prefix += " environment variable: ";
      return error->with_prefix(prefix);
    }
    env_cfg.tags = std::move(*tags);
  }

  if (auto startup_env = lookup(environment::DD_TRACE_STARTUP_LOGS)) {
    env_cfg.log_on_startup = !falsy(*startup_env);
  }
  if (auto enabled_env = lookup(environment::DD_TRACE_ENABLED)) {
    env_cfg.report_traces = !falsy(*enabled_env);
  }
  if (auto enabled_env =
          lookup(environment::DD_INSTRUMENTATION_TELEMETRY_ENABLED)) {
    env_cfg.report_telemetry = !falsy(*enabled_env);
  }
  if (auto trace_delegate_sampling_env =
          lookup(environment::DD_TRACE_DELEGATE_SAMPLING)) {
    env_cfg.delegate_trace_sampling = !falsy(*trace_delegate_sampling_env);
  }
  if (auto enabled_env =
          lookup(environment::DD_TRACE_128_BIT_TRACEID_GENERATION_ENABLED)) {
    env_cfg.generate_128bit_trace_ids = !falsy(*enabled_env);
  }

  // PropagationStyle
  // Print a warning if a questionable combination of environment variables is
  // defined.
  const auto ts = environment::DD_TRACE_PROPAGATION_STYLE;
  const auto tse = environment::DD_TRACE_PROPAGATION_STYLE_EXTRACT;
  const auto se = environment::DD_PROPAGATION_STYLE_EXTRACT;
  const auto tsi = environment::DD_TRACE_PROPAGATION_STYLE_INJECT;
  const auto si = environment::DD_PROPAGATION_STYLE_INJECT;
  // clang-format off
  /*
           ts    tse   se    tsi   si
           ---   ---   ---   ---   ---
    ts  |  x     warn  warn  warn  warn
        |
    tse |  x     x     warn  ok    ok
        |
    se  |  x     x     x     ok    ok
        |
    tsi |  x     x     x     x     warn
        |
    si  |  x     x     x     x     x
  */
  // In each pair, the first would be overridden by the second.
  const std::pair<environment::Variable, environment::Variable> questionable_combinations[] = {
           {ts, tse}, {ts, se},  {ts, tsi}, {ts, si},

                      {se, tse}, /* ok */   /* ok */

                                 /* ok */   /* ok */

                                            {si, tsi},
  };
  // clang-format on

  const auto warn_message = [](StringView name, StringView value,
                               StringView name_override,
                               StringView value_override) {
    std::string message;
    message += "Both the environment variables ";
    append(message, name);
    message += "=";
    message += json_quoted(value);
    message += " and ";
    append(message, name_override);
    message += "=";
    message += json_quoted(value_override);
    message += " are defined. ";
    append(message, name_override);
    message += " will take precedence.";
    return message;
  };

  for (const auto &[var, var_override] : questionable_combinations) {
    const auto value = lookup(var);
    if (!value) {
      continue;
    }
    const auto value_override = lookup(var_override);
    if (!value_override) {
      continue;
    }

    const auto var_name = name(var);
    const auto var_name_override = name(var_override);

    logger.log_error(Error{
        Error::MULTIPLE_PROPAGATION_STYLE_ENVIRONMENT_VARIABLES,
        warn_message(var_name, *value, var_name_override, *value_override)});
  }

  try {
    const auto global_styles =
        styles_from_env(environment::DD_TRACE_PROPAGATION_STYLE);

    if (auto trace_extraction_styles =
            styles_from_env(environment::DD_TRACE_PROPAGATION_STYLE_EXTRACT)) {
      env_cfg.extraction_styles = std::move(*trace_extraction_styles);
    } else if (auto extraction_styles =
                   styles_from_env(environment::DD_PROPAGATION_STYLE_EXTRACT)) {
      env_cfg.extraction_styles = std::move(*extraction_styles);
    } else {
      env_cfg.extraction_styles = global_styles;
    }

    if (auto trace_injection_styles =
            styles_from_env(environment::DD_TRACE_PROPAGATION_STYLE_INJECT)) {
      env_cfg.injection_styles = std::move(*trace_injection_styles);
    } else if (auto injection_styles =
                   styles_from_env(environment::DD_PROPAGATION_STYLE_INJECT)) {
      env_cfg.injection_styles = std::move(*injection_styles);
    } else {
      env_cfg.injection_styles = global_styles;
    }
  } catch (Error &error) {
    return std::move(error);
  }

  return env_cfg;
}

}  // namespace

Expected<FinalizedTracerConfig> finalize_config(const TracerConfig &config) {
  return finalize_config(config, default_clock);
}

Expected<FinalizedTracerConfig> finalize_config(const TracerConfig &user_config,
                                                const Clock &clock) {
  auto logger =
      user_config.logger ? user_config.logger : std::make_shared<CerrLogger>();

  Expected<TracerConfig> env_config = load_tracer_env_config(*logger);
  if (auto error = env_config.if_error()) {
    return *error;
  }

  FinalizedTracerConfig final_config;
  final_config.clock = clock;
  final_config.logger = logger;

  ConfigMetadata::Origin origin;

  std::tie(origin, final_config.defaults.service) =
      pick(env_config->service, user_config.service, "");

  if (final_config.defaults.service.empty()) {
    return Error{Error::SERVICE_NAME_REQUIRED, "Service name is required."};
  }

  final_config.metadata[ConfigName::SERVICE_NAME] = ConfigMetadata(
      ConfigName::SERVICE_NAME, final_config.defaults.service, origin);

  final_config.defaults.service_type =
      value_or(env_config->service_type, user_config.service_type, "web");

  // DD_ENV
  std::tie(origin, final_config.defaults.environment) =
      pick(env_config->environment, user_config.environment, "");
  final_config.metadata[ConfigName::SERVICE_ENV] = ConfigMetadata(
      ConfigName::SERVICE_ENV, final_config.defaults.environment, origin);

  // DD_VERSION
  std::tie(origin, final_config.defaults.version) =
      pick(env_config->version, user_config.version, "");
  final_config.metadata[ConfigName::SERVICE_VERSION] = ConfigMetadata(
      ConfigName::SERVICE_VERSION, final_config.defaults.version, origin);

  final_config.defaults.name = value_or(env_config->name, user_config.name, "");

  // DD_TAGS
  std::tie(origin, final_config.defaults.tags) =
      pick(env_config->tags, user_config.tags,
           std::unordered_map<std::string, std::string>{});
  final_config.metadata[ConfigName::TAGS] = ConfigMetadata(
      ConfigName::TAGS, join_tags(final_config.defaults.tags), origin);

  // Extraction Styles
  const std::vector<PropagationStyle> default_propagation_styles{
      PropagationStyle::DATADOG, PropagationStyle::W3C};

  std::tie(origin, final_config.extraction_styles) =
      pick(env_config->extraction_styles, user_config.extraction_styles,
           default_propagation_styles);
  if (final_config.extraction_styles.empty()) {
    return Error{Error::MISSING_SPAN_EXTRACTION_STYLE,
                 "At least one extraction style must be specified."};
  }
  final_config.metadata[ConfigName::EXTRACTION_STYLES] = ConfigMetadata(
      ConfigName::EXTRACTION_STYLES,
      join_propagation_styles(final_config.extraction_styles), origin);

  // Injection Styles
  std::tie(origin, final_config.injection_styles) =
      pick(env_config->injection_styles, user_config.injection_styles,
           default_propagation_styles);
  if (final_config.injection_styles.empty()) {
    return Error{Error::MISSING_SPAN_INJECTION_STYLE,
                 "At least one injection style must be specified."};
  }
  final_config.metadata[ConfigName::INJECTION_STYLES] = ConfigMetadata(
      ConfigName::INJECTION_STYLES,
      join_propagation_styles(final_config.injection_styles), origin);

  // Startup Logs
  std::tie(origin, final_config.log_on_startup) =
      pick(env_config->log_on_startup, user_config.log_on_startup, true);
  final_config.metadata[ConfigName::STARTUP_LOGS] = ConfigMetadata(
      ConfigName::STARTUP_LOGS, to_string(final_config.log_on_startup), origin);

  // Report traces
  std::tie(origin, final_config.report_traces) =
      pick(env_config->report_traces, user_config.report_traces, true);
  final_config.metadata[ConfigName::REPORT_TRACES] = ConfigMetadata(
      ConfigName::REPORT_TRACES, to_string(final_config.report_traces), origin);

  // Report telemetry
  std::tie(origin, final_config.report_telemetry) =
      pick(env_config->report_telemetry, user_config.report_telemetry, true);
  final_config.metadata[ConfigName::REPORT_TELEMETRY] =
      ConfigMetadata(ConfigName::REPORT_TELEMETRY,
                     to_string(final_config.report_traces), origin);

  // Report hostname
  final_config.report_hostname =
      value_or(env_config->report_hostname, user_config.report_hostname, false);

  // Delegate Sampling
  std::tie(origin, final_config.delegate_trace_sampling) =
      pick(env_config->delegate_trace_sampling,
           user_config.delegate_trace_sampling, false);
  final_config.metadata[ConfigName::DELEGATE_SAMPLING] =
      ConfigMetadata(ConfigName::DELEGATE_SAMPLING,
                     to_string(final_config.delegate_trace_sampling), origin);

  // Tags Header Size
  final_config.tags_header_size = value_or(
      env_config->max_tags_header_size, user_config.max_tags_header_size, 512);

  // 128b Trace IDs
  std::tie(origin, final_config.generate_128bit_trace_ids) =
      pick(env_config->generate_128bit_trace_ids,
           user_config.generate_128bit_trace_ids, true);
  final_config.metadata[ConfigName::GENEREATE_128BIT_TRACE_IDS] =
      ConfigMetadata(ConfigName::GENEREATE_128BIT_TRACE_IDS,
                     to_string(final_config.delegate_trace_sampling), origin);

  // Integration name & version
  final_config.integration_name =
      value_or(env_config->integration_name, user_config.integration_name, "");
  final_config.integration_version = value_or(
      env_config->integration_version, user_config.integration_version, "");

  if (user_config.runtime_id) {
    final_config.runtime_id = user_config.runtime_id;
  }

  if (!user_config.collector) {
    auto finalized =
        finalize_config(user_config.agent, final_config.logger, clock);
    if (auto *error = finalized.if_error()) {
      return std::move(*error);
    }
    final_config.collector = *finalized;
    final_config.metadata.merge(finalized->metadata);
  } else {
    final_config.collector = user_config.collector;
  }

  if (auto trace_sampler_config = finalize_config(user_config.trace_sampler)) {
    final_config.metadata.merge(trace_sampler_config->metadata);
    final_config.trace_sampler = std::move(*trace_sampler_config);
  } else {
    return std::move(trace_sampler_config.error());
  }

  if (auto span_sampler_config =
          finalize_config(user_config.span_sampler, *logger)) {
    final_config.metadata.merge(span_sampler_config->metadata);
    final_config.span_sampler = std::move(*span_sampler_config);
  } else {
    return std::move(span_sampler_config.error());
  }

  return final_config;
}

}  // namespace tracing
}  // namespace datadog
