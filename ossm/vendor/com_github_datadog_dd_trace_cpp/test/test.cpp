#include "test.h"

namespace std {

std::ostream& operator<<(
    std::ostream& stream,
    const std::pair<const std::string, std::string>& item) {
  return stream << '{' << item.first << ", " << item.second << '}';
}

std::ostream& operator<<(
    std::ostream& stream,
    const datadog::tracing::Optional<datadog::tracing::StringView>& item) {
  return stream << item.value_or("<nullopt>");
}

std::ostream& operator<<(std::ostream& stream,
                         const std::optional<int>& maybe) {
  if (maybe) {
    return stream << *maybe;
  }
  return stream << "<nullopt>";
}

}  // namespace std

namespace datadog {
namespace tracing {

std::ostream& operator<<(std::ostream& stream, TraceID trace_id) {
  return stream << "0x" << trace_id.hex_padded();
}

}  // namespace tracing
}  // namespace datadog
