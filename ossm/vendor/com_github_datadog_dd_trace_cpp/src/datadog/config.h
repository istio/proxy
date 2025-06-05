#pragma once

#include "error.h"
#include "optional.h"

namespace datadog {
namespace tracing {

// Enumerates available configuration names for the tracing library
enum class ConfigName : char {
  SERVICE_NAME,
  SERVICE_ENV,
  SERVICE_VERSION,
  REPORT_TRACES,
  TAGS,
  EXTRACTION_STYLES,
  INJECTION_STYLES,
  STARTUP_LOGS,
  REPORT_TELEMETRY,
  DELEGATE_SAMPLING,
  GENEREATE_128BIT_TRACE_IDS,
  AGENT_URL,
  RC_POLL_INTERVAL,
  TRACE_SAMPLING_RATE,
  TRACE_SAMPLING_LIMIT,
  TRACE_SAMPLING_RULES,
  SPAN_SAMPLING_RULES,
};

// Represents metadata for configuration parameters
struct ConfigMetadata {
  enum class Origin : char {
    ENVIRONMENT_VARIABLE,  // Originating from environment variables
    CODE,                  // Defined in code
    REMOTE_CONFIG,         // Retrieved from remote configuration
    DEFAULT                // Default value
  };

  // Name of the configuration parameter
  ConfigName name;
  // Value of the configuration parameter
  std::string value;
  // Origin of the configuration parameter
  Origin origin;
  // Optional error associated with the configuration parameter
  Optional<Error> error;

  ConfigMetadata() = default;
  ConfigMetadata(ConfigName n, std::string v, Origin orig,
                 Optional<Error> err = nullopt)
      : name(n), value(std::move(v)), origin(orig), error(std::move(err)) {}
};

// Return a pair containing the configuration origin and value of a
// configuration value chosen from one of the specified `from_env`,
// `from_config`, and `fallback`. This function defines the relative precedence
// among configuration values originating from the environment, programmatic
// configuration, and default configuration.
template <typename Value, typename DefaultValue>
std::pair<ConfigMetadata::Origin, Value> pick(const Optional<Value> &from_env,
                                              const Optional<Value> &from_user,
                                              DefaultValue fallback) {
  if (from_env) {
    return {ConfigMetadata::Origin::ENVIRONMENT_VARIABLE, *from_env};
  } else if (from_user) {
    return {ConfigMetadata::Origin::CODE, *from_user};
  }
  return {ConfigMetadata::Origin::DEFAULT, fallback};
}

}  // namespace tracing
}  // namespace datadog
