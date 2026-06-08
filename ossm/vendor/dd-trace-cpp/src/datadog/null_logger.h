#pragma once

// This component provides a class, `NullLogger`, that implements the `Logger`
// interface from `logger.h`.
// `NullLogger` is a no-op logger, meaning it doesn't log anything.
//
// `NullLogger` is the default logger used by `Tracer` unless otherwise
// configured in `TracerConfig`.

#include <datadog/logger.h>

namespace datadog {
namespace tracing {

class NullLogger : public Logger {
 public:
  void log_error(const LogFunc&) override{};
  void log_startup(const LogFunc&) override{};

  void log_error(const Error&) override{};
  void log_error(StringView) override{};
};

}  // namespace tracing
}  // namespace datadog
