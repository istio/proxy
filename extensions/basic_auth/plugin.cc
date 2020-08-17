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

void deniedNoAccessToMethod() {
  sendLocalResponse(
      401, "Request denied by Basic Auth check. Request method not authorized",
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
  // "exact":{
  //    "/products/api/store":{
  //      ["GET"],
  //      ["YWRtaW46YWRtaW4="]
  //    }
  // },
  // "prefix":{
  //    "/reviews":{
  //      ["GET", "POST"],
  //      ["YWRtaW46YWRtaW4=", "AWRtaW46YWRtaW4="]
  //    }
  //  },
  // "suffix":{
  //    "/api/pay":{
  //      ["GET", "POST"],
  //      ["YWRtaW46YWRtaW4=", "AWRtaW46YWRtaW4="]
  //    }
  //  }
  //}
  if (!JsonArrayIterate(
          j, "basic_auth_rules", [&](const json& configuration) -> bool {
            std::string match;
            std::string request_path;
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
            struct MethodsCredentialsRule header;
            if (!JsonArrayIterate(
                    configuration, "request_methods",
                    [&](const json& methods) -> bool {
                      auto method = JsonValueAs<std::string>(methods);
                      if (method.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        LOG_WARN("unexpected method");
                        return false;
                      }
                      header.request_methods.insert(
                          std::string(method.first.value()));
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for request methods.");
              return false;
            }

            if (!JsonArrayIterate(
                    configuration, "credentials",
                    [&](const json& credentials) -> bool {
                      auto credential = JsonValueAs<std::string>(credentials);
                      if (credential.second !=
                          Wasm::Common::JsonParserResultDetail::OK) {
                        LOG_WARN("unexpected credential");
                        return false;
                      }
                      header.encoded_credentials.insert(
                          Base64::encode(credential.first.value().data(),
                                         credential.first.value().size()));
                      return true;
                    })) {
              LOG_WARN("Failed to parse configuration for credentials.");
              return false;
            }
            auto path_iter = basic_auth_configuration_.find(match);
            if (path_iter != basic_auth_configuration_.end() &&
                path_iter->second.find(request_path) !=
                    path_iter->second.end()) {
              for (auto& methods : header.request_methods) {
                basic_auth_configuration_[match][request_path]
                    .request_methods.insert(methods);
              }

              for (auto& creds : header.encoded_credentials) {
                basic_auth_configuration_[match][request_path]
                    .encoded_credentials.insert(creds);
              }
            } else {
              basic_auth_configuration_[match][request_path] = header;
            }
            return true;
          })) {
    LOG_WARN("Failed to parse configuration information.");
  }
  return true;
}

FilterHeadersStatus PluginContext::credentialsCheck(
    const PluginRootContext::MethodsCredentialsRule& rule,
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
  auto exact_match_iter = basic_auth_configuration.find("exact");
  // The following checks if the there's a match pattern present in
  // the container. Then we check the request_path according to the
  // match pattern. Afterwards, we check for the method and finally the
  // credentials.
  if (exact_match_iter != basic_auth_configuration.end() &&
      exact_match_iter->second.find(request_path) !=
          basic_auth_configuration["exact"].end()) {
    auto method = getRequestHeader(":method")->toString();
    const auto& exact_rule = basic_auth_configuration["exact"][request_path];
    auto method_iter = exact_rule.request_methods.find(method);
    auto empty_method_iter = exact_rule.request_methods.find("");
    auto method_size = exact_rule.request_methods.size();
    // Check if request method is part of the methods array.
    if (method_iter != exact_rule.request_methods.end()) {
      auto authorization_header = getRequestHeader("authorization")->toString();
      return credentialsCheck(exact_rule, authorization_header);
      // If request method is not pat of methods array. Check if the methods
      // array is empty. If that's not the case, the request method should have
      // access.
    } else if (method_size == 1 &&
               empty_method_iter != exact_rule.request_methods.end()) {
      deniedNoAccessToMethod();
      return FilterHeadersStatus::StopIteration;
    }
  }
  // If the exact match pattern is not found or the request path is not inside
  // the container, we check inside match pattern prefix values.
  auto prefix_match_iter = basic_auth_configuration.find("prefix");
  if (prefix_match_iter != basic_auth_configuration.end()) {
    // Iterate over prefixes looking if theres a prefix that matches
    // request_path If it does, we do the same method and credential checking as
    // before.
    for (auto& itr : basic_auth_configuration["prefix"]) {
      absl::string_view path = request_path;
      if (absl::ConsumePrefix(&path, itr.first)) {
        auto method = getRequestHeader(":method")->toString();
        const auto& prefix_rule = basic_auth_configuration["prefix"][itr.first];
        auto method_iter = prefix_rule.request_methods.find(method);
        auto empty_method_iter = prefix_rule.request_methods.find("");

        auto method_size = prefix_rule.request_methods.size();

        if (method_iter != prefix_rule.request_methods.end()) {
          auto authorization_header =
              getRequestHeader("authorization")->toString();
          return credentialsCheck(prefix_rule, authorization_header);
        } else if (method_size == 1 &&
                   empty_method_iter != prefix_rule.request_methods.end()) {
          deniedNoAccessToMethod();
          return FilterHeadersStatus::StopIteration;
        }
      }
    }
  }
  // If we didn't find a prefix that matches the request_path we are left to
  // check for suffixes.
  auto suffix_match_iter = basic_auth_configuration.find("suffix");
  if (suffix_match_iter != basic_auth_configuration.end()) {
    // Iterate over suffixes looking if theres a prefix that matches
    // request_path If it does, we do the same method and credential checking as
    // before.
    for (auto& itr : basic_auth_configuration["suffix"]) {
      absl::string_view path = request_path;
      if (absl::ConsumeSuffix(&path, itr.first)) {
        auto method = getRequestHeader(":method")->toString();
        const auto& suffix_rule = basic_auth_configuration["suffix"][itr.first];
        auto method_iter = suffix_rule.request_methods.find(method);
        auto empty_method_iter = suffix_rule.request_methods.find("");
        auto method_size = suffix_rule.request_methods.size();

        if (method_iter != suffix_rule.request_methods.end()) {
          auto authorization_header =
              getRequestHeader("authorization")->toString();
          return credentialsCheck(suffix_rule, authorization_header);
        } else if (method_size == 1 &&
                   empty_method_iter != suffix_rule.request_methods.end()) {
          deniedNoAccessToMethod();
          return FilterHeadersStatus::StopIteration;
        }
      }
    }
  }
  // If there's no match with the request path it means the request path doesn't
  // have any basic auth restriction.
  return FilterHeadersStatus::Continue;
}

#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace BasicAuth
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
