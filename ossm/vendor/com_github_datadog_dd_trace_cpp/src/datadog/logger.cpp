#include "logger.h"

#include "error.h"

namespace datadog {
namespace tracing {

void Logger::log_error(const Error& error) {
  log_error([&](auto& stream) { stream << error; });
}

void Logger::log_error(StringView message) {
  log_error([&](auto& stream) { stream << message; });
}

}  // namespace tracing
}  // namespace datadog
