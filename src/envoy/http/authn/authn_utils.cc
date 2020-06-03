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
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/util/json_util.h"
#include "src/envoy/http/jwt_auth/jwt.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {

// The JWT audience key name
static const std::string kJwtAudienceKey = "aud";
// The JWT issuer key name
static const std::string kJwtIssuerKey = "iss";
// The JWT subject key name
static const std::string kJwtSubjectKey = "sub";
// The JWT authorized presented key name
static const std::string kJwtAzpKey = "azp";
// The key name for the original claims in an exchanged token
static const std::string kExchangedTokenOriginalPayload = "original_claims";
}  // namespace

bool AuthnUtils::ProcessJwtPayload(const std::string& payload_str,
                                   istio::authn::JwtPayload* payload) {
  google::protobuf::Struct payload_obj;
  const auto status = google::protobuf::util::JsonStringToMessage(payload_str, &payload_obj);
  if (!status.ok()) {
    return false;
  }
  ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__,
              payload_obj.DebugString());

  *payload->mutable_raw_claims() = payload_str;

  auto claims = payload->mutable_claims()->mutable_fields();
  // Extract claims as string lists
  for (const auto& pair: payload_obj.fields()) {
    std::vector<std::string> claim_values;
    switch (pair.second.kind_case()) {
    case google::protobuf::Value::kStringValue: {
      auto claim_values = absl::StrSplit(pair.second.string_value(), ' ', absl::SkipEmpty());
      for (const auto claim_value : claim_values) {
        (*claims)[pair.first].mutable_list_value()->add_values()->set_string_value(std::string(claim_value));
      }
      break;
    }
    case google::protobuf::Value::kListValue: {
      auto claim_values = pair.second.list_value().values();
      for (auto claim_value = claim_values.begin(); claim_value != claim_values.end(); ++claim_value) {
        assert(claim_value->kind_case() == google::protobuf::Value::kStringValue);
        (*claims)[pair.first].mutable_list_value()->add_values()->set_string_value(claim_value->string_value());
      }
      break;
    }
    default:
      break;
    }
  }

  // Copy audience to the audience in context.proto
  if (claims->find(kJwtAudienceKey) != claims->end()) {
    for (const auto& v : (*claims)[kJwtAudienceKey].list_value().values()) {
      payload->add_audiences(v.string_value());
    }
  }

  // Build user
  if (claims->find(kJwtIssuerKey) != claims->end() &&
      claims->find(kJwtSubjectKey) != claims->end()) {
    payload->set_user(
        (*claims)[kJwtIssuerKey].list_value().values().Get(0).string_value() + "/" +
        (*claims)[kJwtSubjectKey].list_value().values().Get(0).string_value());
  }
  // Build authorized presenter (azp)
  if (claims->find(kJwtAzpKey) != claims->end()) {
    payload->set_presenter(
        (*claims)[kJwtAzpKey].list_value().values().Get(0).string_value());
  }

  return true;
}

bool AuthnUtils::ExtractOriginalPayload(const std::string& token,
                                        std::string* original_payload) {
  google::protobuf::Struct payload_obj;
  const auto status = google::protobuf::util::JsonStringToMessage(token, &payload_obj);
  if (!status.ok()) {
    return false;
  }
  ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__,
              payload_obj.DebugString());
  const auto& payload_obj_fields = payload_obj.fields();

  if (payload_obj_fields.find(kExchangedTokenOriginalPayload) == payload_obj_fields.end()) {
    return false;
  }

  try {
    auto original_payload_value = *payload_obj_fields.find(kExchangedTokenOriginalPayload);
    *original_payload = original_payload_value.second.DebugString();
    ENVOY_LOG(debug, "{}: the original payload in exchanged token is {}",
              __FUNCTION__, *original_payload);
  } catch (...) {
    ENVOY_LOG(debug,
              "{}: original_payload in exchanged token is of invalid format.",
              __FUNCTION__);
    return false;
  }

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
