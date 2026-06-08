#include "test.h"

namespace datadog {
namespace tracing {

std::ostream& operator<<(std::ostream& stream, TraceID trace_id) {
  return stream << "0x" << trace_id.hex_padded();
}

}  // namespace tracing
}  // namespace datadog
