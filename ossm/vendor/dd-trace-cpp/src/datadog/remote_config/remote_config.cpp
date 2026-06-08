#include "remote_config.h"

#include <datadog/remote_config/capability.h>
#include <datadog/remote_config/listener.h>
#include <datadog/string_view.h>

#include <cassert>
#include <regex>
#include <unordered_set>

#include "base64.h"
#include "json.hpp"
#include "random.h"

using namespace datadog::tracing;
using namespace nlohmann::literals;

namespace datadog {
namespace remote_config {
namespace {

constexpr std::array<uint8_t, sizeof(uint64_t)> capabilities_byte_array(
    uint64_t in) {
  std::size_t j = sizeof(in) - 1;
  std::array<uint8_t, sizeof(uint64_t)> res{};
  for (std::size_t i = 0; i < sizeof(in); ++i) {
    res[j--] = static_cast<uint8_t>(in >> (i * 8));
  }

  return res;
}

struct ConfigKeyMetadata final {
  product::Flag product;
  StringView config_id;
};

Optional<ConfigKeyMetadata> parse_config_path(StringView config_path) {
  static const std::regex path_reg(
      "^(datadog/\\d+|employee)/([^/]+)/([^/]+)/[^/]+$");

  std::cmatch match;
  if (!std::regex_match(config_path.data(),
                        config_path.data() + config_path.size(), match,
                        path_reg)) {
    return nullopt;
  }

  assert(match.ready());
  assert(match.size() == 4);

  StringView product_sv(config_path.data() + match.position(2),
                        std::size_t(match.length(2)));
  StringView config_id_sv{config_path.data() + match.position(3),
                          static_cast<std::size_t>(match.length(3))};

  return {{parse_product(product_sv), config_id_sv}};
}

}  // namespace

Manager::Manager(const TracerSignature& tracer_signature,
                 const std::vector<std::shared_ptr<Listener>>& listeners,
                 const std::shared_ptr<tracing::Logger>& logger)
    : tracer_signature_(tracer_signature),
      listeners_(listeners),
      logger_(logger),
      client_id_(uuid()) {
  Capabilities capabilities = 0;

  for (const auto& listener : listeners_) {
    visit_products(
        listener->get_products(), [this, &listener](product::Flag product) {
          products_.emplace(to_string_view(product));
          listeners_per_product_[product].emplace_back(listener.get());
        });
    capabilities |= listener->get_capabilities();
  }

  // The ".client.capabilities" field of the remote config request payload
  // describes which parts of the library's configuration are supported for
  // remote configuration.
  //
  // It's a bitset, 64 bits wide, where each bit indicates whether the library
  // supports a particular feature for remote configuration.
  //
  // The bitset is encoded in the request as a JSON array of 8 integers, where
  // each integer is one byte from the 64 bits. The bytes are in big-endian
  // order within the array.
  capabilities_ = capabilities_byte_array(capabilities);
}

bool Manager::is_new_config(StringView config_path,
                            const nlohmann::json& config_meta) {
  auto it = applied_config_.find(std::string{config_path});
  if (it == applied_config_.cend()) return true;

  return it->second.hash !=
         config_meta.at("/hashes/sha256"_json_pointer).get<StringView>();
}

void Manager::error(std::string message) {
  logger_->log_error(Error{Error::REMOTE_CONFIGURATION_INVALID_INPUT, message});
  state_.error_message = std::move(message);
}

nlohmann::json Manager::make_request_payload() {
  // clang-format off
  auto j = nlohmann::json{
    {"client", {
      {"id", client_id_},
      {"products", products_},
      {"is_tracer", true},
      {"capabilities", capabilities_},
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

  if (state_.error_message) {
    j["client"]["state"]["has_error"] = true;
    j["client"]["state"]["error"] = *state_.error_message;
  }

  if (!applied_config_.empty()) {
    auto config_states = nlohmann::json::array();
    auto cached_target_files = nlohmann::json::array();

    for (const auto& [_, config] : applied_config_) {
      nlohmann::json config_state = {
          {"id", config.id},
          {"version", config.version},
          {"product", to_string_view(config.product)},
          {"apply_state", config.state},
      };

      if (config.error_message) {
        config_state["apply_error"] = *config.error_message;
      }

      config_states.emplace_back(std::move(config_state));

      nlohmann::json cached_file = {
          {"path", config.path},
          {"length", config.content.size()},
          {"hashes", {{{"algorithm", "sha256"}, {"hash", config.hash}}}}};

      cached_target_files.emplace_back(std::move(cached_file));
    }

    j["cached_target_files"] = cached_target_files;
    j["client"]["state"]["config_states"] = config_states;
  }

  return j;
}

void Manager::process_response(const nlohmann::json& json) {
  state_.error_message = nullopt;

  try {
    const auto targets = nlohmann::json::parse(
        base64_decode(json.at("targets").get<StringView>()));

    const auto client_configs_it = json.find("client_configs");

    // `client_configs` is absent => remove previously applied configuration if
    // any applied.
    if (client_configs_it == json.cend()) {
      if (!applied_config_.empty()) {
        std::for_each(applied_config_.cbegin(), applied_config_.cend(),
                      [this](const auto& it) {
                        for (const auto& listener :
                             listeners_per_product_[it.second.product]) {
                          listener->on_revert(it.second);
                        }
                      });
        applied_config_.clear();
      }

      for (const auto& listener : listeners_) {
        listener->on_post_process();
      }

      state_.targets_version =
          targets.at("/signed/version"_json_pointer).get<std::uint64_t>();
      state_.opaque_backend_state =
          targets.at("/signed/custom/opaque_backend_state"_json_pointer);
      return;
    }

    // Keep track of config path received to know which ones to revert.
    std::unordered_set<std::string> visited_config;

    for (const auto& client_config : *client_configs_it) {
      auto config_path = client_config.get<StringView>();
      visited_config.emplace(config_path);

      const auto config_key_metadata = parse_config_path(config_path);
      if (!config_key_metadata) {
        std::string reason{config_path};
        reason += " is an invalid configuration path";

        error(reason);
        return;
      }

      const auto product = config_key_metadata->product;

      const auto& config_metadata =
          targets.at("/signed/targets"_json_pointer).at(config_path);

      if (!is_new_config(config_path, config_metadata)) {
        continue;
      }

      const auto& target_files = json.at("/target_files"_json_pointer);
      auto target_it = std::find_if(
          target_files.cbegin(), target_files.cend(),
          [&config_path](const nlohmann::json& j) {
            return j.at("/path"_json_pointer).get<StringView>() == config_path;
          });

      if (target_it == target_files.cend()) {
        std::string reason{"Target \""};
        append(reason, config_path);
        reason += "\" missing from the list of targets";

        error(reason);
        return;
      }

      auto raw_data = target_it->at("raw").get<StringView>();
      auto decoded_config = base64_decode(raw_data);

      Configuration new_config;
      new_config.id = std::string{config_key_metadata->config_id};
      new_config.path = std::string{config_path};
      new_config.hash = config_metadata.at("/hashes/sha256"_json_pointer);
      new_config.content = std::move(decoded_config);
      new_config.version = config_metadata.at("/custom/v"_json_pointer);
      new_config.product = product;

      for (const auto& listener : listeners_per_product_[product]) {
        // Q: Two listeners on the same product. What should be the behaviour
        // if one of the listeners report an error?
        // R(@dmehala): Unspecified. For now, the config is marked with an
        // error.
        if (auto error_msg = listener->on_update(new_config)) {
          new_config.state = Configuration::State::error;
          new_config.error_message = std::move(*error_msg);
        } else {
          new_config.state = Configuration::State::acknowledged;
        }
      }

      applied_config_[std::string{config_path}] = new_config;
    }

    // Revert applied configurations not present
    for (auto it = applied_config_.cbegin(); it != applied_config_.cend();) {
      if (!visited_config.count(it->first)) {
        for (const auto& listener :
             listeners_per_product_[it->second.product]) {
          listener->on_revert(it->second);
        }
        it = applied_config_.erase(it);
      } else {
        it++;
      }
    }

    for (const auto& listener : listeners_) {
      listener->on_post_process();
    }
    state_.targets_version =
        targets.at("/signed/version"_json_pointer).get<std::uint64_t>();
    state_.opaque_backend_state =
        targets.at("/signed/custom/opaque_backend_state"_json_pointer);
  } catch (const nlohmann::json::exception& json_exception) {
    std::string reason = "Failed to parse the response: ";
    reason += json_exception.what();

    error(reason);
  } catch (const std::exception& e) {
    error(e.what());
  }
}

}  // namespace remote_config
}  // namespace datadog
