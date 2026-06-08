#pragma once

#include <string>

namespace datadog {
namespace telemetry {

namespace details {
enum class MetricType : char { counter, rate, distribution };
}

/// TODO: pre-compute hash?
template <details::MetricType T>
struct Metric final {
  /// The type of the metric.
  static constexpr details::MetricType type = T;
  /// The name of the metric that will be published. A transformation occurs
  /// based on the name and whether it is "common" or "language-specific" when
  /// it is recorded.
  std::string name;
  /// Namespace of the metric.
  std::string scope;
  /// This affects the transformation of the metric name, where it can be a
  /// common telemetry metric, or a language-specific metric that is prefixed
  /// with the language name.
  bool common;
};

using Counter = Metric<details::MetricType::counter>;
using Rate = Metric<details::MetricType::rate>;
using Distribution = Metric<details::MetricType::distribution>;

}  // namespace telemetry
}  // namespace datadog
