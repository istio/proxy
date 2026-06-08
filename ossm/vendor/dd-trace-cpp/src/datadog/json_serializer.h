#pragma once

#include <datadog/span_matcher.h>

#include "json.hpp"

namespace datadog {
namespace tracing {

inline void to_json(nlohmann::json& j, const SpanMatcher& matcher) {
  j["service"] = matcher.service;
  j["name"] = matcher.name;
  j["resource"] = matcher.resource;
  j["tags"] = matcher.tags;
}

inline Expected<SpanMatcher> from_json(const nlohmann::json& json) {
  SpanMatcher result;

  if (json.is_object() == false) {
    std::string message;
    message += "A rule must be a JSON object, but this is of type \"";
    message += json.type_name();
    message += "\": ";
    message += json.dump();
    return Error{Error::RULE_WRONG_TYPE, std::move(message)};
  }

  const auto check_property_type =
      [&json](StringView property, const nlohmann::json& value,
              StringView expected_type) -> Optional<Error> {
    const StringView type = value.type_name();
    if (type == expected_type) {
      return nullopt;
    }

    std::string message;
    message += "Rule property \"";
    append(message, property);
    message += "\" should have type \"";
    append(message, expected_type);
    message += "\", but has type \"";
    append(message, type);
    message += "\": ";
    message += value.dump();
    message += " in rule ";
    message += json.dump();
    return Error{Error::RULE_PROPERTY_WRONG_TYPE, std::move(message)};
  };

  for (const auto& [key, value] : json.items()) {
    if (key == "service") {
      if (auto error = check_property_type(key, value, "string")) {
        return *error;
      }
      result.service = value;
    } else if (key == "name") {
      if (auto error = check_property_type(key, value, "string")) {
        return *error;
      }
      result.name = value;
    } else if (key == "resource") {
      if (auto error = check_property_type(key, value, "string")) {
        return *error;
      }
      result.resource = value;
    } else if (key == "tags") {
      if (auto error = check_property_type(key, value, "object")) {
        return *error;
      }
      for (const auto& [tag_name, tag_value] : value.items()) {
        if (tag_value.is_string() == false) {
          std::string message;
          message += "Rule tag pattern must be a string, but ";
          message += tag_value.dump();
          message += " has type \"";
          message += tag_value.type_name();
          message += "\" for tag named \"";
          message += tag_name;
          message += "\" in rule: ";
          message += json.dump();
          return Error{Error::RULE_TAG_WRONG_TYPE, std::move(message)};
        }
        result.tags.emplace(std::string(tag_name), std::string(tag_value));
      }
    } else {
      // Unknown properties are OK. `SpanMatcher` is used as a base class for
      // trace sampling rules and span sampling rules.  Those derived types
      // will have additional properties in their JSON representations.
    }
  }

  return result;
}

}  // namespace tracing
}  // namespace datadog
