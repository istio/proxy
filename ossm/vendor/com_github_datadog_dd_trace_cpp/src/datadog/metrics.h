#pragma once

// This component provides an interface, `Metric`, and specific classes for
// Counter and Gauge metrics. A metric has a name, type, and set of key:value
// tags associated with it. Metrics can be general to APM or language-specific.
// General metrics have `common` set to `true`, and language-specific metrics
// have `common` set to `false`.

#include <atomic>
#include <string>
#include <vector>

namespace datadog {
namespace tracing {

class Metric {
  // The name of the metric that will be published. A transformation occurs
  // based on the name and whether it is "common" or "language-specific" when it
  // is recorded.
  std::string name_;
  // The type of the metric. This will currently be count or gauge.
  std::string type_;
  // Tags associated with this specific instance of the metric.
  std::vector<std::string> tags_;
  // This affects the transformation of the metric name, where it can be a
  // common telemetry metric, or a language-specific metric that is prefixed
  // with the language name.
  bool common_;

 protected:
  std::atomic<uint64_t> value_ = 0;
  Metric(std::string name, std::string type, std::vector<std::string> tags,
         bool common);

 public:
  // Accessors for name, type, tags, common and capture_and_reset_value are used
  // when producing the JSON message for reporting metrics.
  std::string name();
  std::string type();
  std::vector<std::string> tags();
  bool common();
  uint64_t value();
  uint64_t capture_and_reset_value();
};

// A count metric is used for measuring activity, and has methods for adding a
// number of actions, or incrementing the current number of actions by 1.
class CounterMetric : public Metric {
 public:
  CounterMetric(std::string name, std::vector<std::string> tags, bool common);
  void inc();
  void add(uint64_t amount);
};

// A gauge metric is used for measuring state, and mas methods to set the
// current state, add or subtract from it, or increment/decrement the current
// state by 1.
class GaugeMetric : public Metric {
 public:
  GaugeMetric(std::string name, std::vector<std::string> tags, bool common);
  void set(uint64_t value);
  void inc();
  void add(uint64_t amount);
  void dec();
  void sub(uint64_t amount);
};

}  // namespace tracing
}  // namespace datadog
