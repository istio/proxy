#pragma once

#include <datadog/optional.h>

#include <cassert>
#include <cstdlib>
#include <string>

namespace datadog::test {

// For the lifetime of this object, set a specified environment variable.
// Restore any previous value (or unset the value if it was unset) afterward.
class EnvGuard {
  std::string name_;
  tracing::Optional<std::string> former_value_;

 public:
  EnvGuard(std::string name, std::string value) : name_(std::move(name)) {
    const char* current = std::getenv(name_.c_str());
    if (current) {
      former_value_ = current;
    }
    set_value(value);
  }

  ~EnvGuard() {
    if (former_value_) {
      set_value(*former_value_);
    } else {
      unset();
    }
  }

  void set_value(const std::string& value) {
#ifdef _MSC_VER
    std::string envstr{name_};
    envstr += "=";
    envstr += value;
    assert(_putenv(envstr.c_str()) == 0);
#else
    const bool overwrite = true;
    ::setenv(name_.c_str(), value.c_str(), overwrite);
#endif
  }

  void unset() {
#ifdef _MSC_VER
    std::string envstr{name_};
    envstr += "=";
    assert(_putenv(envstr.c_str()) == 0);
#else
    ::unsetenv(name_.c_str());
#endif
  }
};

}  // namespace datadog::test
