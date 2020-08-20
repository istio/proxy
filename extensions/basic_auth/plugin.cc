/* Copyright 2020 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "extensions/basic_auth/plugin.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "extensions/common/json_util.h"

#ifndef NULL_PLUGIN

#include "extensions/metadata_exchange/base64.h"

#else

#include "common/common/base64.h"

namespace proxy_wasm {
namespace null_plugin {
namespace BasicAuth {
namespace Plugin {

PROXY_WASM_NULL_PLUGIN_REGISTRY;

using Base64 = Envoy::Base64;

#endif

using ::nlohmann::json;
using ::Wasm::Common::JsonArrayIterate;
using ::Wasm::Common::JsonGetField;
using ::Wasm::Common::JsonObjectIterate;
using ::Wasm::Common::JsonValueAs;

static RegisterContextFactory register_BasicAuth(
    CONTEXT_FACTORY(PluginContext), ROOT_FACTORY(PluginRootContext));

namespace {
void deniedNoBasicAuthData() {
  sendLocalResponse(401,
                    "Request denied by Basic Auth check. No Basic "
                    "Authentication information found.",
                    "", {});
}

void deniedInvalidCredentials() {
  sendLocalResponse(401,
                    "Request denied by Basic Auth check. Invalid "
                    "username and/or password",
                    "", {});
}
}  // namespace

FilterHeadersStatus PluginRootContext::credentialsCheck(
    const PluginRootContext::BasicAuthConfigRule& rule,
    std::string_view authorization_header) {
  // Check if the Basic auth header starts with "Basic "
  if (!absl::StartsWith(authorization_header, "Basic ")) {
    deniedNoBasicAuthData();
    return FilterHeadersStatus::StopIteration;
  }
  std::string_view authorization_header_strip =
      absl::StripPrefix(authorization_header, "Basic ");

  auto auth_credential_iter =
      rule.encoded_credentials.find(std::string(authorization_header_strip));
  // Check if encoded credential is part of the encoded_credentials
  // set from our container to grant or deny access.
  if (auth_credential_iter == rule.encoded_credentials.end()) {
    deniedInvalidCredentials();
    return FilterHeadersStatus::StopIteration;
  }

  return FilterHeadersStatus::Continue;
}

bool PluginRootContext::onConfigure(size_t size) {
  // Parse configuration JSON string.
  if (size > 0 && !configure(size)) {
    LOG_WARN("configuration has errors initialization will not continue.");
    return false;
  }
  return true;
}

bool PluginRootContext::configure(size_t configuration_size) {
  auto configuration_data = getBufferBytes(WasmBufferType::PluginConfiguration,
                                           0, configuration_size);
  // Parse configuration JSON string.
  auto result = ::Wasm::Common::JsonParse(configuration_data->view());
  if (!result.has_value()) {
    LOG_WARN(absl::StrCat("cannot parse plugin configuration JSON string: ",
                          configuration_data->view()));
    return false;
  }
  // j is a JsonObject holds configuration data
  auto j = result.value();
  // basic_auth_configuration_ container has the following example structure
  //{
  // "GET":{
  //    { "/products",
  //      "prefix",
  //      ["YWRtaW46YWRtaW4="]
  //    },
  //    { "/products/store",
  //      "exact",
  //      ["FRtaW46YWRtaW4=", "ARtaW46YWRW4="]
  //    }
  // },
  // "POST":{
  //     { "/wiki",
  //      "prefix",
  //      ["YWRtaW46YWRtaW4=", "AWRtaW46YWRtaW4="]
  //    }
  //  },
  // "DELETE":{
  //    { "/api/store/product/id/two",
  //      "exact",
  //      ["AWRtaW46YWRtaW4="]
  //    }
  //  }
  //}
  if (!JsonArrayIterate(
          j, "basic_auth_rules", [&](const json& configuration) -> bool {
            std::string match;
            std::string request_path;
            std::vector<std::string> request_methods;
            if (!JsonObjectIterate(
                    configuration, "request_path",
                    [&](std::string pattern) -> bool {
                      match = pattern;
                      request_path = JsonGetField<std::string>(
                                         configuration["request_path"], pattern)
                                         .value_or("");
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for request path.");
              return false;
            }
            if (request_path == "") {
              LOG_WARN("Path inside request_path field is empty.");
              return false;
            }
            if (match != "prefix" && match != "exact" && match != "suffix") {
              LOG_WARN(
                  absl::StrCat("match_pattern: ", match, " is not valid."));
              return false;
            }

            if (!JsonArrayIterate(
                    configuration, "request_methods",
                    [&](const json& method) -> bool {
                      auto method_string = JsonValueAs<std::string>(method);
                      if (method_string.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        return false;
                      }
                      request_methods.push_back(method_string.first.value());
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for request methods.");
              return false;
            }
            struct BasicAuthConfigRule rule;
            if (!JsonArrayIterate(
                    configuration, "credentials",
                    [&](const json& credentials) -> bool {
                      auto credential = JsonValueAs<std::string>(credentials);
                      if (credential.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        return false;
                      }
                      rule.encoded_credentials.insert(
                          Base64::encode(credential.first.value().data(),
                                         credential.first.value().size()));
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for credentials.");
              return false;
            }

            if (match == "prefix") {
              rule.pattern = Prefix;
            } else if (match == "exact") {
              rule.pattern = Exact;
            } else if (match == "suffix") {
              rule.pattern = Suffix;
            }
            rule.request_path = request_path;
            for (auto& method : request_methods) {
              basic_auth_configuration_[method].push_back(rule);
            }
            return true;
          })) {
    LOG_WARN(absl::StrCat("cannot parse plugin configuration JSON string: ",
                          configuration_data->view()));
  }
  return true;
}

FilterHeadersStatus PluginRootContext::check() {
  auto request_path_header = getRequestHeader(":path");
  std::string_view request_path = request_path_header->view();
  auto method = getRequestHeader(":method")->toString();
  auto method_iter = basic_auth_configuration_.find(method);
  // First we check if the request method is present in our container
  if (method_iter != basic_auth_configuration_.end()) {
    // We iterate through our vector of struct in order to find if the
    // request_path according to given match pattern, is part of the plugin's
    // configuration data. If that's the case we check the credentials
    FilterHeadersStatus header_status = FilterHeadersStatus::Continue;
    auto authorization_header = getRequestHeader("authorization");
    std::string_view authorization = authorization_header->view();
    for (auto& rules : basic_auth_configuration_[method]) {
      if (rules.pattern == MATCH_TYPE::Prefix) {
        if (absl::StartsWith(request_path, rules.request_path)) {
          header_status = credentialsCheck(rules, authorization);
        }
      } else if (rules.pattern == MATCH_TYPE::Exact) {
        if (rules.request_path == request_path) {
          header_status = credentialsCheck(rules, authorization);
        }
      } else if (rules.pattern == MATCH_TYPE::Suffix) {
        if (absl::EndsWith(request_path, rules.request_path)) {
          header_status = credentialsCheck(rules, authorization);
        }
      }
      if (header_status == FilterHeadersStatus::StopIteration) {
        return FilterHeadersStatus::StopIteration;
      }
    }
  }
  // If there's no match against the request method or request path it means
  // that they don't have any basic auth restriction.
  return FilterHeadersStatus::Continue;
}

FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t, bool) {
  return rootContext()->check();
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace BasicAuth
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
