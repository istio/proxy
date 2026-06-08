#pragma once

// The `ConfigManager` class is designed to handle configuration update
// and provide access to the current configuration.
// It utilizes a mutex to ensure thread safety when updating or accessing
// the configuration.

#include <datadog/clock.h>
#include <datadog/optional.h>
#include <datadog/remote_config/listener.h>
#include <datadog/span_defaults.h>
#include <datadog/tracer_config.h>

#include <mutex>

#include "json.hpp"

namespace datadog {
namespace tracing {

class ConfigManager : public remote_config::Listener {
 public:
  // The `Update` struct serves as a container for configuration that can
  // exclusively be changed remotely.
  //
  // Configurations can be `nullopt` to signal the absence of a value from the
  // remote configuration value.
  struct Update final {
    Optional<bool> report_traces;
    Optional<Rate> trace_sampling_rate;
    Optional<std::unordered_map<std::string, std::string>> tags;
    Optional<std::vector<TraceSamplerRule>> trace_sampling_rules;
  };

 private:
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
  ~ConfigManager() override{};

  remote_config::Products get_products() override;
  remote_config::Capabilities get_capabilities() override;

  Optional<std::string> on_update(
      const Listener::Configuration& config) override;
  void on_revert(const Listener::Configuration& config) override;
  void on_post_process() override{};

  // Return the `TraceSampler` consistent with the most recent configuration.
  std::shared_ptr<TraceSampler> trace_sampler();

  // Return the `SpanDefaults` consistent with the most recent configuration.
  std::shared_ptr<const SpanDefaults> span_defaults();

  // Return whether traces should be sent to the collector.
  bool report_traces();

  // Return a JSON representation of the current configuration managed by this
  // object.
  nlohmann::json config_json() const;

  void apply_update(const ConfigManager::Update& conf);
};

}  // namespace tracing
}  // namespace datadog
