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

  enum class Level { TRACE, DEBUG, INFO, WARN, ERROR };

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

#define MIXER_TRACE_ENABLED (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::TRACE))
#define MIXER_DEBUG_ENABLED (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::DEBUG))
#define MIXER_INFO_ENABLED (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::INFO))
#define MIXER_WARN_ENABLED (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::WARN))
#define MIXER_ERROR_ENABLED (istio::utils::getLogger().isLoggable(istio::utils::Logger::Level::ERROR))

#define MIXER_TRACE(FORMAT, ...) \
  istio::utils::getLogger().log(istio::utils::Logger::Level::TRACE, FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_DEBUG(FORMAT, ...) \
  istio::utils::getLogger().log(istio::utils::Logger::Level::DEBUG, FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_INFO(FORMAT, ...) \
  istio::utils::getLogger().log(istio::utils::Logger::Level::INFO, FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_WARN(FORMAT, ...) \
  istio::utils::getLogger().log(istio::utils::Logger::Level::WARN, FILE_LINE FORMAT, ##__VA_ARGS__)
#define MIXER_ERROR(FORMAT, ...) \
  istio::utils::getLogger().log(istio::utils::Logger::Level::ERROR, FILE_LINE FORMAT, ##__VA_ARGS__)
