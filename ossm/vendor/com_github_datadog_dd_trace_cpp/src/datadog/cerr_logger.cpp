#include "cerr_logger.h"

#include <iostream>

namespace datadog {
namespace tracing {

void CerrLogger::log_error(const LogFunc& write) { log(write); }

void CerrLogger::log_startup(const LogFunc& write) { log(write); }

void CerrLogger::log(const LogFunc& write) {
  std::lock_guard<std::mutex> lock(mutex_);

  stream_.clear();
  // Copy an empty string in, don't move it.
  // We want `stream_` to keep its storage.
  const std::string empty;
  stream_.str(empty);

  write(stream_);
  stream_ << '\n';
  std::cerr << stream_.str();
}

}  // namespace tracing
}  // namespace datadog
