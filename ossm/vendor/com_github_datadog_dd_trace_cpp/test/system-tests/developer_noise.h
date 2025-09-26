#pragma once

#include <datadog/logger.h>

#include <memory>
#include <mutex>
#include <sstream>

class DeveloperNoiseLogger : public datadog::tracing::Logger {
  bool developer_noise_ = false;
  std::mutex mutex_;

 public:
  void developer_noise(bool enabled);
  void log_info(datadog::tracing::StringView message);
  void log_error(const LogFunc&) override;
  void log_startup(const LogFunc&) override;
  using Logger::log_error;  // other overloads of log_error

 private:
  void make_noise(std::string_view level, const LogFunc&);
};

std::shared_ptr<DeveloperNoiseLogger> make_logger();
