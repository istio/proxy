#include "developer_noise.h"

#include <chrono>
#include <iomanip>
#include <iostream>

void DeveloperNoiseLogger::developer_noise(bool enabled) {
  developer_noise_ = enabled;
}

void DeveloperNoiseLogger::log_info(datadog::tracing::StringView message) {
  if (developer_noise_) {
    make_noise("INFO", [&](auto& stream) { stream << message; });
  }
}

void DeveloperNoiseLogger::log_error(const LogFunc& insert_to_stream) {
  make_noise("ERROR", insert_to_stream);
}

void DeveloperNoiseLogger::log_startup(const LogFunc& insert_to_stream) {
  make_noise("INFO", insert_to_stream);
}

void DeveloperNoiseLogger::make_noise(std::string_view level,
                                      const LogFunc& insert_to_stream) {
  std::time_t t = std::time(0);  // get time now
  auto now = std::localtime(&t);

  std::lock_guard<std::mutex> lock(mutex_);

  std::cerr << "[" << std::put_time(now, "%c") << "] [" << level << "] ";
  insert_to_stream(std::cerr);
  std::cerr << std::endl;
}

std::shared_ptr<DeveloperNoiseLogger> make_logger() {
  // Initialize a logger that handles errors from the library as well as
  // comforting developer-noise from the tracing service.
  auto logger = std::make_shared<DeveloperNoiseLogger>();

  // Enable developer noise when a specific environment variable is set.
  auto verbose_env = std::getenv("CPP_PARAMETRIC_TEST_VERBOSE");
  if (verbose_env) {
    try {
      if (std::stoi(verbose_env) == 1) {
        logger->developer_noise(true);
      }
    } catch (...) {
    }
  }

  return logger;
}
