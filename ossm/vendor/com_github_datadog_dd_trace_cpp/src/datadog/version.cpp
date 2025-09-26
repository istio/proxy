#include "version.h"

namespace datadog {
namespace tracing {

#define DD_TRACE_VERSION "v0.2.2"

const char* const tracer_version = DD_TRACE_VERSION;
const char* const tracer_version_string =
    "[dd-trace-cpp version " DD_TRACE_VERSION "]";

}  // namespace tracing
}  // namespace datadog
