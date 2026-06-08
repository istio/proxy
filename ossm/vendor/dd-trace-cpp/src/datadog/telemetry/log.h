#include <datadog/optional.h>
#include <datadog/string_view.h>

#include <string>

namespace datadog::telemetry {

enum class LogLevel : char { ERROR, WARNING };

struct LogMessage final {
  std::string message;
  LogLevel level;
  tracing::Optional<std::string> stacktrace;
  std::chrono::seconds::rep timestamp;
};

inline tracing::StringView to_string(LogLevel level) {
  switch (level) {
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::WARNING:
      return "WARNING";
  }

  // Unreachable.
  return "";
}

}  // namespace datadog::telemetry
