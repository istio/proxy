/* Copyright 2019 Istio Authors. All Rights Reserved.
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
void deniedNoAccessToCredentials() {
  sendLocalResponse(401,
                    "Request denied by Basic Auth check. No credential "
                    "has access to requested path.",
                    "", {});
}

void deniedNoAuthorizationHeader() {
  sendLocalResponse(401,
                    "Request denied by Basic Auth check. No Authorization "
                    "header was found.",
                    "", {});
}

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
  //    { "/api/products/reviews",
  //      "exact",
  //      ["YWRtaW46YWRtaW4=", "ARtaW46YWRW4="]
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
                      auto path = JsonGetField<std::string>(
                                      configuration["request_path"], pattern)
                                      .value_or("");
                      auto path_string = JsonValueAs<std::string>(path);
                      if (path_string.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        LOG_WARN("unexpected request path");
                        return false;
                      }
                      request_path = path;
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for request path.");
              return false;
            }
            if (request_path == "" || match == "") {
              return false;
            }
            if (!JsonArrayIterate(
                    configuration, "request_methods",
                    [&](const json& methods) -> bool {
                      auto method = JsonValueAs<std::string>(methods);
                      if (method.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        LOG_WARN("unexpected method");
                        return false;
                      }
                      request_methods.push_back(
                          std::string(method.first.value()));
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for request methods.");
              return false;
            }
            struct BasicAuthConfigRule rules;
            if (!JsonArrayIterate(
                    configuration, "credentials",
                    [&](const json& credentials) -> bool {
                      auto credential = JsonValueAs<std::string>(credentials);
                      if (credential.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        LOG_WARN("unexpected credential");
                        return false;
                      }
                      rules.encoded_credentials.insert(
                          Base64::encode(credential.first.value().data(),
                                         credential.first.value().size()));
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for credentials.");
              return false;
            }

            if (match == "prefix") {
              rules.pattern = Prefix;
            } else if (match == "exact") {
              rules.pattern = Exact;
            } else if (match == "suffix") {
              rules.pattern = Suffix;
            }
            rules.request_path = request_path;
            for (auto& methods : request_methods) {
              basic_auth_configuration_[methods].push_back(rules);
            }
            return true;
          })) {
    LOG_WARN("Failed to parse configuration information.");
  }
  return true;
}

FilterHeadersStatus PluginContext::credentialsCheck(
    const PluginRootContext::BasicAuthConfigRule& rule,
    const std::string& authorization_header) {
  auto credential_iter = rule.encoded_credentials.find("");
  auto credential_size = rule.encoded_credentials.size();
  // Check if credential set is of of size 1 and its an empty string.
  if (credential_size == 1 &&
      credential_iter != rule.encoded_credentials.end()) {
    deniedNoAccessToCredentials();
    return FilterHeadersStatus::StopIteration;
  }
  // Check if there is an authorization header
  if (authorization_header.compare("") == 0) {
    deniedNoAuthorizationHeader();
    return FilterHeadersStatus::StopIteration;
  }
  absl::string_view basic_authorization_header = authorization_header;
  // Check if the Basic auth header starts with "Basic "
  if (!absl::ConsumePrefix(&basic_authorization_header, "Basic ")) {
    deniedNoBasicAuthData();
    return FilterHeadersStatus::StopIteration;
  }
  auto auth_credential_iter =
      rule.encoded_credentials.find(std::string(basic_authorization_header));
  // Check if encoded credential is part of the encoded_credentials
  // set from our container to grant or deny access.
  if (auth_credential_iter == rule.encoded_credentials.end()) {
    deniedInvalidCredentials();
    return FilterHeadersStatus::StopIteration;
  } else {
    return FilterHeadersStatus::Continue;
  }
  return FilterHeadersStatus::StopIteration;
}

FilterHeadersStatus PluginContext::onRequestHeaders(uint32_t, bool) {
  auto basic_auth_configuration = rootContext()->basicAuthConfigurationValue();
  auto request_path = getRequestHeader(":path")->toString();
  auto method = getRequestHeader(":method")->toString();
  auto method_iter = basic_auth_configuration.find(method);
  // First we check if the request method is present in our container
  if (method_iter != basic_auth_configuration.end()) {
    // We iterate through our vector of struct in order to find if the
    // request_path according to given match patterns, is part of the plugin's
    // configuration data. If that's the case we check the credentials
    for (auto& rules : basic_auth_configuration[method]) {
      if (rules.pattern == PluginRootContext::MATCH_TYPE::Prefix) {
        absl::string_view path = request_path;
        if (absl::ConsumePrefix(&path, rules.request_path)) {
          auto authorization_header =
              getRequestHeader("authorization")->toString();
          return credentialsCheck(rules, authorization_header);
        }
      } else if (rules.pattern == PluginRootContext::MATCH_TYPE::Exact) {
        if (rules.request_path == request_path) {
          auto authorization_header =
              getRequestHeader("authorization")->toString();
          return credentialsCheck(rules, authorization_header);
        }
      } else if (rules.pattern == PluginRootContext::MATCH_TYPE::Suffix) {
        absl::string_view path = request_path;
        if (absl::ConsumeSuffix(&path, rules.request_path)) {
          auto authorization_header =
              getRequestHeader("authorization")->toString();
          return credentialsCheck(rules, authorization_header);
        }
      }
    }
  }
  // If there's no match against the request method or request path it means
  // that they don't have any basic auth restriction.
  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace BasicAuth
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
