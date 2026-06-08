#pragma once

// Remote Configuration is a Datadog capability that allows a user to remotely
// configure and change the behaviour of the tracing library.
// The current implementation is restricted to Application Performance
// Monitoring features.
//
// The `RemoteConfigurationManager` class implement the protocol to query,
// process and verify configuration from a remote source. It is also
// responsible for handling configuration updates received from a remote source
// and maintains the state of applied configuration.
// It interacts with the `ConfigManager` to seamlessly apply or revert
// configurations based on responses received from the remote source.

#include <datadog/logger.h>
#include <datadog/optional.h>
#include <datadog/remote_config/listener.h>
#include <datadog/runtime_id.h>
#include <datadog/string_view.h>
#include <datadog/trace_sampler_config.h>
#include <datadog/tracer_signature.h>

#include <array>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "json.hpp"

namespace datadog {
namespace remote_config {

class Manager {
  // Represents the *current* state of the Manager.
  // It is also used to report errors to the remote source.
  struct State {
    uint64_t targets_version = 0;
    std::string opaque_backend_state;
    tracing::Optional<std::string> error_message;
  };

  // Holds information about a specific configuration update,
  // including its identifier, hash value, version number and the content.
  struct Configuration final : public Listener::Configuration {
    enum State : char {
      unacknowledged = 1,
      acknowledged = 2,
      error = 3
    } state = State::unacknowledged;

    std::string hash;
    tracing::Optional<std::string> error_message;
  };

  tracing::TracerSignature tracer_signature_;
  std::vector<std::shared_ptr<Listener>> listeners_;
  std::shared_ptr<tracing::Logger> logger_;
  std::set<tracing::StringView> products_;
  std::unordered_map<product::Flag, std::vector<Listener*>>
      listeners_per_product_;
  std::array<uint8_t, sizeof(uint64_t)> capabilities_;
  std::string client_id_;

  State state_;
  std::unordered_map<std::string, Configuration> applied_config_;

 public:
  Manager(const tracing::TracerSignature& tracer_signature,
          const std::vector<std::shared_ptr<Listener>>& listeners,
          const std::shared_ptr<tracing::Logger>& logger);

  // Construct a JSON object representing the payload to be sent in a remote
  // configuration request.
  nlohmann::json make_request_payload();

  // Handles the response received from a remote source and udates the internal
  // state accordingly.
  void process_response(const nlohmann::json& json);

 private:
  // Tell if a `config_path` is a new configuration update.
  bool is_new_config(tracing::StringView config_path,
                     const nlohmann::json& config_meta);

  void error(std::string message);
};

}  // namespace remote_config
}  // namespace datadog
