#include <datadog/trace_source.h>

#include "parse_util.h"

namespace datadog {
namespace tracing {

bool validate_trace_source(StringView source_str) {
  if (source_str.size() > 2) return false;

  auto maybe_ts_uint = parse_uint64(source_str, 10);
  if (maybe_ts_uint.if_error()) return false;

  return true;
}

}  // namespace tracing
}  // namespace datadog
