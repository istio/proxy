/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/istio/utils/logger.h"
#include <stdarg.h>
#include <stdio.h>

namespace istio {
namespace utils {

Logger::~Logger() {}

void Logger::log(Level level, const char *format, ...) {
  if (!isLoggable(level)) {
    return;
  }

  va_list args;
  va_start(args, format);
  char buffer[256];
  ::vsnprintf(buffer, sizeof(buffer), format, args);
  buffer[sizeof(buffer) - 1] = 0;
  va_end(args);

  writeBuffer(level, buffer);
}

// This is equivalent to the original mixer client logger, but is not used when
// mixer client is used inside Envoy.  This preserves mixer client's
// independence of the Envoy source code without forcing it to log (infrequenty)
// to stdout.
class DefaultLogger : public Logger {
 protected:
  virtual bool isLoggable(Level level) override {
    switch (level) {
      case Level::TRACE_:
      case Level::DEBUG_:
        return false;
      case Level::INFO_:
      case Level::WARN_:
      case Level::ERROR_:
        return true;
    }
  }

  virtual void writeBuffer(Level level, const char *buffer) override {
    fprintf(stderr, "%s %s\n", levelString(level), buffer);
  }

 private:
  const char *levelString(Level level) {
    switch (level) {
      case Level::TRACE_:
        return "TRACE";
      case Level::DEBUG_:
        return "DEBUG";
      case Level::INFO_:
        return "INFO";
      case Level::WARN_:
        return "WARN";
      case Level::ERROR_:
        return "ERROR";
    }
  }
};

static std::unique_ptr<Logger> active_logger{new DefaultLogger()};

void setLogger(std::unique_ptr<Logger> logger) {
  active_logger = std::move(logger);
  MIXER_INFO("Logger active");
}
Logger &getLogger() { return *active_logger; }

}  // namespace utils
}  // namespace istio
