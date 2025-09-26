#pragma once

#include <datadog/error.h>
#include <datadog/logger.h>

#include <algorithm>
#include <iosfwd>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "../test.h"

using namespace datadog::tracing;

struct NullLogger : public Logger {
  void log_error(const LogFunc&) override {}
  void log_startup(const LogFunc&) override {}
  void log_error(const Error&) override {}
  void log_error(StringView) override {}
};

struct MockLogger : public Logger {
  struct Entry {
    enum Kind { DD_ERROR, STARTUP } kind;
    std::variant<std::string, Error> payload;
  };

  mutable std::mutex mutex;
  std::ostream* echo = nullptr;
  std::vector<Entry> entries;

  enum EchoPolicy {
    ERRORS_ONLY,
    ERRORS_AND_STARTUP,
  } policy;

  explicit MockLogger(std::ostream& echo_stream, EchoPolicy echo_policy)
      : echo(&echo_stream), policy(echo_policy) {}
  MockLogger() = default;

  void log_error(const LogFunc& write) override {
    std::lock_guard<std::mutex> lock{mutex};
    std::ostringstream stream;
    write(stream);
    if (echo) {
      *echo << stream.str() << '\n';
    }
    entries.push_back(Entry{Entry::DD_ERROR, stream.str()});
  }

  void log_startup(const LogFunc& write) override {
    std::lock_guard<std::mutex> lock{mutex};
    std::ostringstream stream;
    write(stream);
    if (echo && policy == ERRORS_AND_STARTUP) {
      *echo << stream.str() << '\n';
    }
    entries.push_back(Entry{Entry::STARTUP, stream.str()});
  }

  void log_error(const Error& error) override {
    std::lock_guard<std::mutex> lock{mutex};
    if (echo) {
      *echo << error << '\n';
    }
    entries.push_back(Entry{Entry::DD_ERROR, error});
  }

  void log_error(StringView message) override {
    std::lock_guard<std::mutex> lock{mutex};
    if (echo) {
      *echo << message << '\n';
    }
    entries.push_back(Entry{Entry::DD_ERROR, std::string(message)});
  }

  int error_count() const { return count(Entry::DD_ERROR); }

  int startup_count() const { return count(Entry::STARTUP); }

  int count(Entry::Kind kind) const {
    std::lock_guard<std::mutex> lock{mutex};
    return std::count_if(
        entries.begin(), entries.end(),
        [kind](const Entry& entry) { return entry.kind == kind; });
  }

  const Error& first_error() const {
    REQUIRE(error_count() > 0);
    std::lock_guard<std::mutex> lock{mutex};
    auto found = std::find_if(
        entries.begin(), entries.end(),
        [](const Entry& entry) { return entry.kind == Entry::DD_ERROR; });
    return std::get<Error>(found->payload);
  }

  const std::string& first_startup() const {
    REQUIRE(startup_count() > 0);
    std::lock_guard<std::mutex> lock{mutex};
    auto found = std::find_if(
        entries.begin(), entries.end(),
        [](const Entry& entry) { return entry.kind == Entry::STARTUP; });
    return std::get<std::string>(found->payload);
  }
};

std::ostream& operator<<(std::ostream&, const std::vector<MockLogger::Entry>&);
