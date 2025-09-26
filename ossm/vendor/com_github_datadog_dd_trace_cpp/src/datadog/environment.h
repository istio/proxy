#pragma once

// This component provides a registry of all environment variables that can be
// used to configure this library.
//
// Each `enum Variable` denotes an environment variable.  The enum value names
// are the same as the names of the environment variables.
//
// `variable_names` is an array of the names of the environment variables. Nginx
// uses `variable_names` as an allow list of environment variables to forward to
// worker processes.
//
// `name` returns the name of a specified `Variable`.
//
// `lookup` retrieves the value of `Variable` in the environment.

#include "json_fwd.hpp"
#include "optional.h"
#include "string_view.h"

namespace datadog {
namespace tracing {
namespace environment {

// To enforce correspondence between `enum Variable` and `variable_names`, the
// preprocessor is used so that the DD_* symbols are listed exactly once.
#define LIST_ENVIRONMENT_VARIABLES(MACRO)       \
  MACRO(DD_AGENT_HOST)                          \
  MACRO(DD_ENV)                                 \
  MACRO(DD_INSTRUMENTATION_TELEMETRY_ENABLED)   \
  MACRO(DD_PROPAGATION_STYLE_EXTRACT)           \
  MACRO(DD_PROPAGATION_STYLE_INJECT)            \
  MACRO(DD_REMOTE_CONFIGURATION_ENABLED)        \
  MACRO(DD_REMOTE_CONFIG_POLL_INTERVAL_SECONDS) \
  MACRO(DD_SERVICE)                             \
  MACRO(DD_SPAN_SAMPLING_RULES)                 \
  MACRO(DD_SPAN_SAMPLING_RULES_FILE)            \
  MACRO(DD_TRACE_DELEGATE_SAMPLING)             \
  MACRO(DD_TRACE_PROPAGATION_STYLE_EXTRACT)     \
  MACRO(DD_TRACE_PROPAGATION_STYLE_INJECT)      \
  MACRO(DD_TRACE_PROPAGATION_STYLE)             \
  MACRO(DD_TAGS)                                \
  MACRO(DD_TRACE_AGENT_PORT)                    \
  MACRO(DD_TRACE_AGENT_URL)                     \
  MACRO(DD_TRACE_DEBUG)                         \
  MACRO(DD_TRACE_ENABLED)                       \
  MACRO(DD_TRACE_RATE_LIMIT)                    \
  MACRO(DD_TRACE_REPORT_HOSTNAME)               \
  MACRO(DD_TRACE_SAMPLE_RATE)                   \
  MACRO(DD_TRACE_SAMPLING_RULES)                \
  MACRO(DD_TRACE_STARTUP_LOGS)                  \
  MACRO(DD_TRACE_TAGS_PROPAGATION_MAX_LENGTH)   \
  MACRO(DD_VERSION)                             \
  MACRO(DD_TRACE_128_BIT_TRACEID_GENERATION_ENABLED)

#define WITH_COMMA(ARG) ARG,

enum Variable { LIST_ENVIRONMENT_VARIABLES(WITH_COMMA) };

// Quoting a macro argument requires this two-step.
#define QUOTED_IMPL(ARG) #ARG
#define QUOTED(ARG) QUOTED_IMPL(ARG)

#define QUOTED_WITH_COMMA(ARG) WITH_COMMA(QUOTED(ARG))

inline const char *const variable_names[] = {
    LIST_ENVIRONMENT_VARIABLES(QUOTED_WITH_COMMA)};

#undef QUOTED_WITH_COMMA
#undef QUOTED
#undef QUOTED_IMPL
#undef WITH_COMMA
#undef LIST_ENVIRONMENT_VARIABLES

// Return the name of the specified environment `variable`.
StringView name(Variable variable);

// Return the value of the specified environment `variable`, or return
// `nullopt` if that variable is not set in the environment.
Optional<StringView> lookup(Variable variable);

nlohmann::json to_json();

}  // namespace environment
}  // namespace tracing
}  // namespace datadog
