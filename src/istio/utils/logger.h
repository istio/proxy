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

#pragma once

#include <memory>

namespace istio {
namespace utils {

class Logger {
 public:
  virtual ~Logger();

  enum class Level { TRACE_, DEBUG_, INFO_, WARN_, ERROR_ };

  void log(Level level, const char *format, ...);

  virtual bool isLoggable(Level level) = 0;

 protected:
  virtual void writeBuffer(Level level, const char *buffer) = 0;
};

extern void setLogger(std::unique_ptr<Logger> logger);
extern Logger &getLogger();

}  // namespace utils
}  // namespace istio

#define STRINGLIT2(x) #x
#define STRINGLIT(x) STRINGLIT2(x)
#define FILE_LINE "[" __FILE__ ":" STRINGLIT(__LINE__) "] "

#define MIXER_TRACE_ENABLED \
  (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::TRACE_))
#define MIXER_DEBUG_ENABLED \
  (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::DEBUG_))
#define MIXER_INFO_ENABLED \
  (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::INFO_))
#define MIXER_WARN_ENABLED \
  (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::WARN_))
#define MIXER_ERROR_ENABLED \
  (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::ERROR_))

#define MIXER_TRACE_INT(FORMAT, ...)                                 \
  istio::utils::getLogger().log(istio::utils::Logger::Level::TRACE_, \
                                FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_DEBUG_INT(FORMAT, ...)                                 \
  istio::utils::getLogger().log(istio::utils::Logger::Level::DEBUG_, \
                                FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_INFO_INT(FORMAT, ...)                                 \
  istio::utils::getLogger().log(istio::utils::Logger::Level::INFO_, \
                                FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_WARN_INT(FORMAT, ...)                                 \
  istio::utils::getLogger().log(istio::utils::Logger::Level::WARN_, \
                                FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_ERROR_INT(FORMAT, ...)                                 \
  istio::utils::getLogger().log(istio::utils::Logger::Level::ERROR_, \
                                FILE_LINE FORMAT, ##__VA_ARGS__)

#define MIXER_TRACE(FORMAT, ...)              \
  do {                                        \
    if (MIXER_TRACE_ENABLED) {                \
      MIXER_TRACE_INT(FORMAT, ##__VA_ARGS__); \
    }                                         \
  } while (0)

#define MIXER_DEBUG(FORMAT, ...)              \
  do {                                        \
    if (MIXER_DEBUG_ENABLED) {                \
      MIXER_DEBUG_INT(FORMAT, ##__VA_ARGS__); \
    }                                         \
  } while (0)

#define MIXER_INFO(FORMAT, ...)              \
  do {                                       \
    if (MIXER_INFO_ENABLED) {                \
      MIXER_INFO_INT(FORMAT, ##__VA_ARGS__); \
    }                                        \
  } while (0)

#define MIXER_WARN(FORMAT, ...)              \
  do {                                       \
    if (MIXER_WARN_ENABLED) {                \
      MIXER_WARN_INT(FORMAT, ##__VA_ARGS__); \
    }                                        \
  } while (0)

#define MIXER_ERROR(FORMAT, ...)              \
  do {                                        \
    if (MIXER_ERROR_ENABLED) {                \
      MIXER_ERROR_INT(FORMAT, ##__VA_ARGS__); \
    }                                         \
  } while (0)
