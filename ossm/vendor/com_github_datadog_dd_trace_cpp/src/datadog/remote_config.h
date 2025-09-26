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

#include <memory>
#include <string>

#include "config_manager.h"
#include "logger.h"
#include "optional.h"
#include "runtime_id.h"
#include "string_view.h"
#include "trace_sampler_config.h"
#include "tracer_signature.h"

namespace datadog {
namespace tracing {

class RemoteConfigurationManager {
  // Represents the *current* state of the RemoteConfigurationManager.
  // It is also used to report errors to the remote source.
  struct State {
    uint64_t targets_version = 0;
    std::string opaque_backend_state;
    Optional<std::string> error_message;
  };

  // Holds information about a specific configuration update,
  // including its identifier, hash value, version number and the content.
  struct Configuration {
    std::string id;
    std::string hash;
    std::size_t version;
    ConfigUpdate content;

    enum State : char {
      unacknowledged = 1,
      acknowledged = 2,
      error = 3
    } state = State::unacknowledged;

    Optional<std::string> error_message;
  };

  TracerSignature tracer_signature_;
  std::shared_ptr<ConfigManager> config_manager_;
  std::string client_id_;

  State state_;
  std::unordered_map<std::string, Configuration> applied_config_;

 public:
  RemoteConfigurationManager(
      const TracerSignature& tracer_signature,
      const std::shared_ptr<ConfigManager>& config_manager);

  // Construct a JSON object representing the payload to be sent in a remote
  // configuration request.
  nlohmann::json make_request_payload();

  // Handles the response received from a remote source and udates the internal
  // state accordingly.
  std::vector<ConfigMetadata> process_response(const nlohmann::json& json);

 private:
  // Tell if a `config_path` is a new configuration update.
  bool is_new_config(StringView config_path, const nlohmann::json& config_meta);

  // Apply a remote configuration.
  std::vector<ConfigMetadata> apply_config(Configuration config);

  // Revert a remote configuration.
  std::vector<ConfigMetadata> revert_config(Configuration config);
};

}  // namespace tracing
}  // namespace datadog
