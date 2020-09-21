/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include "authn_utils.h"

#include <regex>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "extensions/common/wasm/json_util.h"
#include "google/protobuf/struct.pb.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {
// The JWT audience key name
static const std::string kJwtAudienceKey = "aud";
// The JWT issuer key name
static const std::string kJwtIssuerKey = "iss";
// The key name for the original claims in an exchanged token
static const std::string kExchangedTokenOriginalPayload = "original_claims";

};  // namespace

bool AuthnUtils::ProcessJwtPayload(const std::string& payload_str,
                                   istio::authn::JwtPayload* payload) {
  auto result = Wasm::Common::JsonParse(payload_str);
  if (!result.has_value()) {
    return false;
  }
  auto json_obj = result.value();
  ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__, json_obj.dump());

  *payload->mutable_raw_claims() = payload_str;

  auto claims = payload->mutable_claims()->mutable_fields();
  // Extract claims as string lists
  Wasm::Common::JsonObjectIterate(
      json_obj, [&json_obj, &claims](const std::string& key) -> bool {
        // In current implementation, only string/string list objects are
        // extracted
        std::vector<absl::string_view> list;
        auto field_value =
            Wasm::Common::JsonGetField<std::vector<absl::string_view>>(json_obj,
                                                                       key);
        if (field_value.detail() != Wasm::Common::JsonParserResultDetail::OK) {
          auto str_field_value =
              Wasm::Common::JsonGetField<absl::string_view>(json_obj, key);
          if (str_field_value.detail() !=
              Wasm::Common::JsonParserResultDetail::OK) {
            return true;
          }
          list = absl::StrSplit(str_field_value.value().data(), ' ',
                                absl::SkipEmpty());
        } else {
          list = field_value.value();
        }
        for (auto& s : list) {
          (*claims)[key].mutable_list_value()->add_values()->set_string_value(
              std::string(s));
        }
        return true;
      });

  // Copy audience to the audience in context.proto
  if (claims->find(kJwtAudienceKey) != claims->end()) {
    for (const auto& v : (*claims)[kJwtAudienceKey].list_value().values()) {
      payload->add_audiences(v.string_value());
    }
  }

  // Build user
  if (claims->find("iss") != claims->end() &&
      claims->find("sub") != claims->end()) {
    payload->set_user(
        (*claims)["iss"].list_value().values().Get(0).string_value() + "/" +
        (*claims)["sub"].list_value().values().Get(0).string_value());
  }
  // Build authorized presenter (azp)
  if (claims->find("azp") != claims->end()) {
    payload->set_presenter(
        (*claims)["azp"].list_value().values().Get(0).string_value());
  }

  return true;
}

bool AuthnUtils::ExtractOriginalPayload(const std::string& token,
                                        std::string* original_payload) {
  auto result = Wasm::Common::JsonParse(token);
  if (!result.has_value()) {
    return false;
  }
  auto json_obj = result.value();

  if (!json_obj.contains(kExchangedTokenOriginalPayload)) {
    return false;
  }

  auto original_payload_obj =
      Wasm::Common::JsonGetField<Wasm::Common::JsonObject>(
          json_obj, kExchangedTokenOriginalPayload);
  if (original_payload_obj.detail() !=
      Wasm::Common::JsonParserResultDetail::OK) {
    ENVOY_LOG(debug,
              "{}: original_payload in exchanged token is of invalid format.",
              __FUNCTION__);
    return false;
  }
  *original_payload = original_payload_obj.value().dump();

  return true;
}

bool AuthnUtils::MatchString(absl::string_view str,
                             const iaapi::StringMatch& match) {
  switch (match.match_type_case()) {
    case iaapi::StringMatch::kExact: {
      return match.exact() == str;
    }
    case iaapi::StringMatch::kPrefix: {
      return absl::StartsWith(str, match.prefix());
    }
    case iaapi::StringMatch::kSuffix: {
      return absl::EndsWith(str, match.suffix());
    }
    case iaapi::StringMatch::kRegex: {
      return std::regex_match(std::string(str), std::regex(match.regex()));
    }
    default:
      return false;
  }
}

static bool matchRule(absl::string_view path,
                      const iaapi::Jwt_TriggerRule& rule) {
  for (const auto& excluded : rule.excluded_paths()) {
    if (AuthnUtils::MatchString(path, excluded)) {
      // The rule is not matched if any of excluded_paths matched.
      return false;
    }
  }

  if (rule.included_paths_size() > 0) {
    for (const auto& included : rule.included_paths()) {
      if (AuthnUtils::MatchString(path, included)) {
        // The rule is matched if any of included_paths matched.
        return true;
      }
    }

    // The rule is not matched if included_paths is not empty and none of them
    // matched.
    return false;
  }

  // The rule is matched if none of excluded_paths matched and included_paths is
  // empty.
  return true;
}

bool AuthnUtils::ShouldValidateJwtPerPath(absl::string_view path,
                                          const iaapi::Jwt& jwt) {
  // If the path is empty which shouldn't happen for a HTTP request or if
  // there are no trigger rules at all, then simply return true as if there're
  // no per-path jwt support.
  if (path == "" || jwt.trigger_rules_size() == 0) {
    return true;
  }
  for (const auto& rule : jwt.trigger_rules()) {
    if (matchRule(path, rule)) {
      return true;
    }
  }
  return false;
}

}  // namespace AuthN
}  // namespace Istio
}  // namespace Http
}  // namespace Envoy
