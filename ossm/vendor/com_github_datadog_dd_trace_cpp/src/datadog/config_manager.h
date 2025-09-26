#pragma once

// The `ConfigManager` class is designed to handle configuration update
// and provide access to the current configuration.
// It utilizes a mutex to ensure thread safety when updating or accessing
// the configuration.

#include <mutex>

#include "clock.h"
#include "config_update.h"
#include "json.hpp"
#include "optional.h"
#include "span_defaults.h"
#include "tracer_config.h"

namespace datadog {
namespace tracing {

class ConfigManager {
  // A class template for managing dynamic configuration values.
  //
  // This class allows storing and managing dynamic configuration values. It
  // maintains an original value and a current value, allowing for updates and
  // resets.
  //
  // Additionally, it provides methods for accessing the current value and
  // checking whether it has been modified from its original state.
  template <typename Value>
  class DynamicConfig {
    Value original_value_;
    Optional<Value> current_value_;

   public:
    // Constructs a DynamicConf object with the given initial value
    explicit DynamicConfig(Value original_value)
        : original_value_(std::move(original_value)) {}

    // Resets the current value of the configuration to the original value
    void reset() { current_value_ = nullopt; }

    // Returns whether the current value is the original value
    bool is_original_value() const { return !current_value_.has_value(); }

    const Value& value() const {
      return current_value_.has_value() ? *current_value_ : original_value_;
    }

    // Updates the current value of the configuration
    void operator=(const Value& rhs) { current_value_ = rhs; }
  };

  mutable std::mutex mutex_;
  Clock clock_;
  std::unordered_map<ConfigName, ConfigMetadata> default_metadata_;

  std::shared_ptr<TraceSampler> trace_sampler_;
  std::vector<TraceSamplerRule> rules_;

  DynamicConfig<std::shared_ptr<const SpanDefaults>> span_defaults_;
  DynamicConfig<bool> report_traces_;

 private:
  template <typename T>
  void reset_config(ConfigName name, T& conf,
                    std::vector<ConfigMetadata>& metadata);

 public:
  ConfigManager(const FinalizedTracerConfig& config);

  // Return the `TraceSampler` consistent with the most recent configuration.
  std::shared_ptr<TraceSampler> trace_sampler();

  // Return the `SpanDefaults` consistent with the most recent configuration.
  std::shared_ptr<const SpanDefaults> span_defaults();

  // Return whether traces should be sent to the collector.
  bool report_traces();

  // Apply the specified `conf` update.
  std::vector<ConfigMetadata> update(const ConfigUpdate& conf);

  // Restore the configuration that was passed to this object's constructor,
  // overriding any previous calls to `update`.
  std::vector<ConfigMetadata> reset();

  // Return a JSON representation of the current configuration managed by this
  // object.
  nlohmann::json config_json() const;
};

}  // namespace tracing
}  // namespace datadog
