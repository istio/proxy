#include "remote_config.h"

#include <cassert>
#include <cstdint>
#include <type_traits>
#include <unordered_set>

#include "base64.h"
#include "json.hpp"
#include "random.h"
#include "string_view.h"
#include "version.h"

using namespace nlohmann::literals;

namespace datadog {
namespace tracing {
namespace {

// The ".client.capabilities" field of the remote config request payload
// describes which parts of the library's configuration are supported for remote
// configuration.
//
// It's a bitset, 64 bits wide, where each bit indicates whether the library
// supports a particular feature for remote configuration.
//
// The bitset is encoded in the request as a JSON array of 8 integers, where
// each integer is one byte from the 64 bits. The bytes are in big-endian order
// within the array.
enum CapabilitiesFlag : uint64_t {
  APM_TRACING_SAMPLE_RATE = 1 << 12,
  APM_TRACING_TAGS = 1 << 15,
  APM_TRACING_ENABLED = 1 << 19,
  APM_TRACING_SAMPLE_RULES = 1 << 29,
};

constexpr std::array<uint8_t, sizeof(uint64_t)> capabilities_byte_array(
    uint64_t in) {
  std::size_t j = sizeof(in) - 1;
  std::array<uint8_t, sizeof(uint64_t)> res{};
  for (std::size_t i = 0; i < sizeof(in); ++i) {
    res[j--] = static_cast<uint8_t>(in >> (i * 8));
  }

  return res;
}

constexpr std::array<uint8_t, sizeof(uint64_t)> k_apm_capabilities =
    capabilities_byte_array(APM_TRACING_SAMPLE_RATE | APM_TRACING_TAGS |
                            APM_TRACING_ENABLED | APM_TRACING_SAMPLE_RULES);

constexpr StringView k_apm_product = "APM_TRACING";
constexpr StringView k_apm_product_path_substring = "/APM_TRACING/";

ConfigUpdate parse_dynamic_config(const nlohmann::json& j) {
  ConfigUpdate config_update;

  if (auto sampling_rate_it = j.find("tracing_sampling_rate");
      sampling_rate_it != j.cend() && sampling_rate_it->is_number()) {
    config_update.trace_sampling_rate = sampling_rate_it->get<double>();
  }

  if (auto tags_it = j.find("tracing_tags");
      tags_it != j.cend() && tags_it->is_array()) {
    config_update.tags = tags_it->get<std::vector<StringView>>();
  }

  if (auto tracing_enabled_it = j.find("tracing_enabled");
      tracing_enabled_it != j.cend() && tracing_enabled_it->is_boolean()) {
    config_update.report_traces = tracing_enabled_it->get<bool>();
  }

  if (auto tracing_sampling_rules_it = j.find("tracing_sampling_rules");
      tracing_sampling_rules_it != j.cend() &&
      tracing_sampling_rules_it->is_array()) {
    config_update.trace_sampling_rules = &(*tracing_sampling_rules_it);
  }

  return config_update;
}

}  // namespace

RemoteConfigurationManager::RemoteConfigurationManager(
    const TracerSignature& tracer_signature,
    const std::shared_ptr<ConfigManager>& config_manager)
    : tracer_signature_(tracer_signature),
      config_manager_(config_manager),
      client_id_(uuid()) {
  assert(config_manager_);
}

bool RemoteConfigurationManager::is_new_config(
    StringView config_path, const nlohmann::json& config_meta) {
  auto it = applied_config_.find(std::string{config_path});
  if (it == applied_config_.cend()) return true;

  return it->second.hash !=
         config_meta.at("/hashes/sha256"_json_pointer).get<StringView>();
}

nlohmann::json RemoteConfigurationManager::make_request_payload() {
  // clang-format off
  auto j = nlohmann::json{
    {"client", {
      {"id", client_id_},
      {"products", nlohmann::json::array({k_apm_product})},
      {"is_tracer", true},
      {"capabilities", k_apm_capabilities},
      {"client_tracer", {
        {"runtime_id", tracer_signature_.runtime_id.string()},
        {"language", tracer_signature_.library_language},
        {"tracer_version", tracer_signature_.library_version},
        {"service", tracer_signature_.default_service},
        {"env", tracer_signature_.default_environment}
      }},
      {"state", {
        {"root_version", 1},
        {"targets_version", state_.targets_version},
        {"backend_client_state", state_.opaque_backend_state}
      }}
    }}
  };
  // clang-format on

  if (!applied_config_.empty()) {
    auto config_states = nlohmann::json::array();
    for (const auto& [_, config] : applied_config_) {
      nlohmann::json config_state = {
          {"id", config.id},
          {"version", config.version},
          {"product", k_apm_product},
          {"apply_state", config.state},
      };

      if (config.error_message) {
        config_state["apply_error"] = *config.error_message;
      }

      config_states.emplace_back(std::move(config_state));
    }

    j["client"]["state"]["config_states"] = config_states;
  }

  if (state_.error_message) {
    j["client"]["state"]["has_error"] = true;
    j["client"]["state"]["error"] = *state_.error_message;
  }

  return j;
}

std::vector<ConfigMetadata> RemoteConfigurationManager::process_response(
    const nlohmann::json& json) {
  std::vector<ConfigMetadata> config_updates;
  config_updates.reserve(8);

  state_.error_message = nullopt;

  try {
    const auto targets = nlohmann::json::parse(
        base64_decode(json.at("targets").get<StringView>()));

    state_.targets_version = targets.at("/signed/version"_json_pointer);
    state_.opaque_backend_state =
        targets.at("/signed/custom/opaque_backend_state"_json_pointer);

    const auto client_configs_it = json.find("client_configs");

    // `client_configs` is absent => remove previously applied configuration if
    // any applied.
    if (client_configs_it == json.cend()) {
      if (!applied_config_.empty()) {
        std::for_each(applied_config_.cbegin(), applied_config_.cend(),
                      [this, &config_updates](const auto it) {
                        auto updated = revert_config(it.second);
                        config_updates.insert(config_updates.end(),
                                              updated.begin(), updated.end());
                      });
        applied_config_.clear();
      }
      return config_updates;
    }

    // Keep track of config path received to know which ones to revert.
    std::unordered_set<std::string> visited_config;
    visited_config.reserve(client_configs_it->size());

    for (const auto& client_config : *client_configs_it) {
      auto config_path = client_config.get<StringView>();
      visited_config.emplace(config_path);

      const auto& config_metadata =
          targets.at("/signed/targets"_json_pointer).at(config_path);
      if (!contains(config_path, k_apm_product_path_substring) ||
          !is_new_config(config_path, config_metadata)) {
        continue;
      }

      const auto& target_files = json.at("/target_files"_json_pointer);
      auto target_it = std::find_if(
          target_files.cbegin(), target_files.cend(),
          [&config_path](const nlohmann::json& j) {
            return j.at("/path"_json_pointer).get<StringView>() == config_path;
          });

      if (target_it == target_files.cend()) {
        state_.error_message =
            "Missing configuration from Remote Configuration response: No "
            "target file having path \"";
        append(*state_.error_message, config_path);
        *state_.error_message += '\"';
        return config_updates;
      }

      const auto config_json = nlohmann::json::parse(
          base64_decode(target_it.value().at("raw").get<StringView>()));

      Configuration new_config;
      new_config.id = config_json.at("id");
      new_config.hash = config_metadata.at("/hashes/sha256"_json_pointer);
      new_config.version = config_json.at("revision");

      const auto& targeted_service = config_json.at("service_target");
      if (targeted_service.at("service").get<StringView>() !=
              tracer_signature_.default_service ||
          targeted_service.at("env").get<StringView>() !=
              tracer_signature_.default_environment) {
        new_config.state = Configuration::State::error;
        new_config.error_message = "Wrong service targeted";
      } else {
        new_config.state = Configuration::State::acknowledged;
        new_config.content = parse_dynamic_config(config_json.at("lib_config"));

        auto updated = apply_config(new_config);
        config_updates.insert(config_updates.end(), updated.begin(),
                              updated.end());
      }

      applied_config_[std::string{config_path}] = new_config;
    }

    // Applied configuration not present must be reverted.
    for (auto it = applied_config_.cbegin(); it != applied_config_.cend();) {
      if (!visited_config.count(it->first)) {
        auto updated = revert_config(it->second);
        config_updates.insert(config_updates.end(), updated.begin(),
                              updated.end());
        it = applied_config_.erase(it);
      } else {
        it++;
      }
    }
  } catch (const nlohmann::json::exception& e) {
    std::string error_message = "Ill-formatted Remote Configuration response: ";
    error_message += e.what();

    state_.error_message = std::move(error_message);
    return config_updates;
  }

  return config_updates;
}

std::vector<ConfigMetadata> RemoteConfigurationManager::apply_config(
    Configuration config) {
  return config_manager_->update(config.content);
}

std::vector<ConfigMetadata> RemoteConfigurationManager::revert_config(
    Configuration) {
  return config_manager_->reset();
}

}  // namespace tracing
}  // namespace datadog
