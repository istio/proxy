#include "span_matcher.h"

#include <algorithm>

#include "error.h"
#include "glob.h"
#include "json.hpp"
#include "optional.h"
#include "span_data.h"

namespace datadog {
namespace tracing {
namespace {

bool is_match(StringView pattern, StringView subject) {
  // Since "*" is the default pattern, optimize for that case.
  return pattern == "*" || glob_match(pattern, subject);
}

}  // namespace

nlohmann::json SpanMatcher::to_json() const {
  return nlohmann::json::object({
      {"service", service},
      {"name", name},
      {"resource", resource},
      {"tags", tags},
  });
}

bool SpanMatcher::match(const SpanData& span) const {
  return is_match(service, span.service) && is_match(name, span.name) &&
         is_match(resource, span.resource) &&
         std::all_of(tags.begin(), tags.end(), [&](const auto& entry) {
           const auto& [name, pattern] = entry;
           auto found = span.tags.find(name);
           return found != span.tags.end() && is_match(pattern, found->second);
         });
}

Expected<SpanMatcher> SpanMatcher::from_json(const nlohmann::json& json) {
  SpanMatcher result;

  std::string type = json.type_name();
  if (type != "object") {
    std::string message;
    message += "A rule must be a JSON object, but this is of type \"";
    message += type;
    message += "\": ";
    message += json.dump();
    return Error{Error::RULE_WRONG_TYPE, std::move(message)};
  }

  const auto check_property_type =
      [&](StringView property, const nlohmann::json& value,
          StringView expected_type) -> Optional<Error> {
    type = value.type_name();
    if (type == expected_type) {
      return nullopt;
    }

    std::string message;
    message += "Rule property \"";
    append(message, property);
    message += "\" should have type \"";
    append(message, expected_type);
    message += "\", but has type \"";
    message += type;
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
        type = tag_value.type_name();
        if (type != "string") {
          std::string message;
          message += "Rule tag pattern must be a string, but ";
          message += tag_value.dump();
          message += " has type \"";
          message += type;
          message += "\" for tag named \"";
          message += tag_name;
          message += "\" in rule: ";
          message += json.dump();
          return Error{Error::RULE_TAG_WRONG_TYPE, std::move(message)};
        }
        result.tags.emplace(std::string(tag_name), std::string(tag_value));
      }
    } else {
      // Unknown properties are OK.  `SpanMatcher` is used as a base class for
      // trace sampling rules and span sampling rules.  Those derived types
      // will have additional properties in their JSON representations.
    }
  }

  return result;
}

}  // namespace tracing
}  // namespace datadog
