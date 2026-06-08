#pragma once

#include <datadog/config.h>
#include <datadog/optional.h>

#include <string>
#include <unordered_map>

namespace datadog::telemetry {

/// Represents a single product and its associated metadata.
struct Product final {
  enum class Name : char {
    tracing,
    appsec,
    profiler,
    mlobs,
    live_debugger,
    rum,
  };

  /// The product name identifier from one of the possible values above.
  Name name;
  /// Flag indicating if the product is currently enabled.
  bool enabled;
  /// The version string of the product.
  std::string version;
  /// Optional error code related to the product status.
  tracing::Optional<int> error_code;
  /// Optional error message related to the product status.
  tracing::Optional<std::string> error_message;
  /// Map of configuration settings for the product.
  std::unordered_map<tracing::ConfigName, tracing::ConfigMetadata>
      configurations;
};

inline std::string_view to_string(Product::Name product) {
  switch (product) {
    case Product::Name::tracing:
      return "tracing";
    case Product::Name::appsec:
      return "appsec";
    case Product::Name::profiler:
      return "profiler";
    case Product::Name::mlobs:
      return "mlobs";
    case Product::Name::live_debugger:
      return "live_debugger";
    case Product::Name::rum:
      return "rum";
  }

  // unreachable.
  return "";
}

}  // namespace datadog::telemetry
