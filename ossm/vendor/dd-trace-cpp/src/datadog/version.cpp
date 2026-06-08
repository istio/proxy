#include <datadog/version.h>

namespace datadog {
namespace tracing {

#define DD_TRACE_VERSION "v2.0.0"

const char* const tracer_version = DD_TRACE_VERSION;
const char* const tracer_version_string =
    "[dd-trace-cpp version " DD_TRACE_VERSION "]";

}  // namespace tracing
}  // namespace datadog
