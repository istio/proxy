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

#include <regex>

#include "authn_utils.h"
#include "common/json/json_loader.h"
#include "google/protobuf/struct.pb.h"
#include "src/envoy/http/jwt_auth/jwt.h"

namespace Envoy {
namespace Http {
namespace Istio {
namespace AuthN {
namespace {
// The JWT audience key name
static const std::string kJwtAudienceKey = "aud";

// Extract JWT claim as a string list.
// This function only extracts string and string list claims.
// A string claim is extracted as a string list of 1 item.
void ExtractStringList(const std::string& key, const Envoy::Json::Object& obj,
                       std::vector<std::string>* list) {
  // First, try as string
  try {
    // Try as string, will throw execption if object type is not string.
    list->push_back(obj.getString(key));
  } catch (Json::Exception& e) {
    // Not convertable to string
  }
  // Next, try as string array
  try {
    std::vector<std::string> vector = obj.getStringArray(key);
    for (const std::string v : vector) {
      list->push_back(v);
    }
  } catch (Json::Exception& e) {
    // Not convertable to string array
  }
}
};  // namespace

bool AuthnUtils::ProcessJwtPayload(const std::string& payload_str,
                                   istio::authn::JwtPayload* payload) {
  Envoy::Json::ObjectSharedPtr json_obj;
  try {
    json_obj = Json::Factory::loadFromString(payload_str);
    ENVOY_LOG(debug, "{}: json object is {}", __FUNCTION__,
              json_obj->asJsonString());
  } catch (...) {
    return false;
  }

  *payload->mutable_raw_claims() = payload_str;

  auto claims = payload->mutable_claims()->mutable_fields();
  // Extract claims as string lists
  json_obj->iterate([json_obj, claims](const std::string& key,
                                       const Json::Object&) -> bool {
    // In current implementation, only string/string list objects are extracted
    std::vector<std::string> list;
    ExtractStringList(key, *json_obj, &list);
    for (auto s : list) {
      (*claims)[key].mutable_list_value()->add_values()->set_string_value(s);
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

bool AuthnUtils::MatchString(const char* const str,
                             const iaapi::StringMatch& match) {
  if (str == nullptr) {
    return false;
  }
  switch (match.match_type_case()) {
    case iaapi::StringMatch::kExact: {
      return match.exact().compare(str) == 0;
    }
    case iaapi::StringMatch::kPrefix: {
      return StringUtil::startsWith(str, match.prefix());
    }
    case iaapi::StringMatch::kSuffix: {
      return StringUtil::endsWith(str, match.suffix());
    }
    case iaapi::StringMatch::kRegex: {
      return std::regex_match(str, std::regex(match.regex()));
    }
    default:
      return false;
  }
}

static bool matchRule(const char* const path,
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

bool AuthnUtils::ShouldValidateJwtPerPath(const char* const path,
                                          const iaapi::Jwt& jwt) {
  // If the path is nullptr which shouldn't happen for a HTTP request or if
  // there are no trigger rules at all, then simply return true as if there're
  // no per-path jwt support.
  if (path == nullptr || jwt.trigger_rules_size() == 0) {
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
