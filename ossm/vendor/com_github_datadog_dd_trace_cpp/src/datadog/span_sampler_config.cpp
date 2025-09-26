#include "span_sampler_config.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "environment.h"
#include "expected.h"
#include "json.hpp"

namespace datadog {
namespace tracing {
namespace {

std::string to_string(const std::vector<SpanSamplerConfig::Rule> &rules) {
  nlohmann::json res;
  for (const auto &r : rules) {
    auto j = r.to_json();
    j["sample_rate"] = r.sample_rate;
    if (r.max_per_second) {
      j["max_per_second"] = *r.max_per_second;
    }
    res.emplace_back(std::move(j));
  }

  return res.dump();
}

// `env_var` is the name of the environment variable from which `rules_raw` was
// obtained.  It's used for error messages.
Expected<std::vector<SpanSamplerConfig::Rule>> parse_rules(StringView rules_raw,
                                                           StringView env_var) {
  std::vector<SpanSamplerConfig::Rule> rules;
  nlohmann::json json_rules;

  try {
    json_rules = nlohmann::json::parse(rules_raw);
  } catch (const nlohmann::json::parse_error &error) {
    std::string message;
    message += "Unable to parse JSON from ";
    append(message, env_var);
    message += " value ";
    append(message, rules_raw);
    message += ": ";
    message += error.what();
    return Error{Error::SPAN_SAMPLING_RULES_INVALID_JSON, std::move(message)};
  }

  std::string type = json_rules.type_name();
  if (type != "array") {
    std::string message;
    message += "Trace sampling rules must be an array, but JSON in ";
    append(message, env_var);
    message += " has type \"";
    message += type;
    message += "\": ";
    append(message, rules_raw);
    return Error{Error::SPAN_SAMPLING_RULES_WRONG_TYPE, std::move(message)};
  }

  const std::unordered_set<std::string> allowed_properties{
      "service", "name", "resource", "tags", "sample_rate", "max_per_second"};

  for (const auto &json_rule : json_rules) {
    auto matcher = SpanMatcher::from_json(json_rule);
    if (auto *error = matcher.if_error()) {
      std::string prefix;
      prefix += "Unable to create a rule from ";
      append(prefix, env_var);
      prefix += " JSON ";
      append(prefix, rules_raw);
      prefix += ": ";
      return error->with_prefix(prefix);
    }

    SpanSamplerConfig::Rule rule{*matcher};

    auto sample_rate = json_rule.find("sample_rate");
    if (sample_rate != json_rule.end()) {
      type = sample_rate->type_name();
      if (type != "number") {
        std::string message;
        message += "Unable to parse a rule from ";
        append(message, env_var);
        message += " JSON ";
        append(message, rules_raw);
        message += ".  The \"sample_rate\" property of the rule ";
        message += json_rule.dump();
        message += " is not a number, but instead has type \"";
        message += type;
        message += "\".";
        return Error{Error::SPAN_SAMPLING_RULES_SAMPLE_RATE_WRONG_TYPE,
                     std::move(message)};
      }
      rule.sample_rate = *sample_rate;
    }

    auto max_per_second = json_rule.find("max_per_second");
    if (max_per_second != json_rule.end()) {
      type = max_per_second->type_name();
      if (type != "number") {
        std::string message;
        message += "Unable to parse a rule from ";
        append(message, env_var);
        message += " JSON ";
        append(message, rules_raw);
        message += ".  The \"max_per_second\" property of the rule ";
        message += json_rule.dump();
        message += " is not a number, but instead has type \"";
        message += type;
        message += "\".";
        return Error{Error::SPAN_SAMPLING_RULES_MAX_PER_SECOND_WRONG_TYPE,
                     std::move(message)};
      }
      rule.max_per_second = *max_per_second;
    }

    // Look for unexpected properties.
    for (const auto &[key, value] : json_rule.items()) {
      if (allowed_properties.count(key)) {
        continue;
      }
      std::string message;
      message += "Unexpected property \"";
      message += key;
      message += "\" having value ";
      message += value.dump();
      message += " in trace sampling rule ";
      message += json_rule.dump();
      message += ".  Error occurred while parsing from ";
      append(message, env_var);
      message += ": ";
      append(message, rules_raw);
      return Error{Error::SPAN_SAMPLING_RULES_UNKNOWN_PROPERTY,
                   std::move(message)};
    }

    rules.emplace_back(std::move(rule));
  }

  return rules;
}

Expected<SpanSamplerConfig> load_span_sampler_env_config(Logger &logger) {
  SpanSamplerConfig env_config;

  auto rules_env = lookup(environment::DD_SPAN_SAMPLING_RULES);
  if (rules_env) {
    auto maybe_rules =
        parse_rules(*rules_env, name(environment::DD_SPAN_SAMPLING_RULES));
    if (auto *error = maybe_rules.if_error()) {
      return std::move(*error);
    }
    env_config.rules = std::move(*maybe_rules);
  }

  if (auto file_env = lookup(environment::DD_SPAN_SAMPLING_RULES_FILE)) {
    if (rules_env) {
      const auto rules_file_name =
          name(environment::DD_SPAN_SAMPLING_RULES_FILE);
      const auto rules_name = name(environment::DD_SPAN_SAMPLING_RULES);
      std::string message;
      append(message, rules_file_name);
      message += " is overridden by ";
      append(message, rules_name);
      message += ".  Since both are set, ";
      append(message, rules_name);
      message += " takes precedence, and ";
      append(message, rules_file_name);
      message += " will be ignored.";
      logger.log_error(message);
    } else {
      const auto span_rules_file = std::string(*file_env);

      const auto file_error = [&](const char *operation) {
        std::string message;
        message += "Unable to ";
        message += operation;
        message += " file \"";
        message += span_rules_file;
        message += "\" specified as value of environment variable ";
        append(message, name(environment::DD_SPAN_SAMPLING_RULES_FILE));

        return Error{Error::SPAN_SAMPLING_RULES_FILE_IO, std::move(message)};
      };

      std::ifstream file(span_rules_file);
      if (!file) {
        return file_error("open");
      }

      std::ostringstream rules_stream;
      rules_stream << file.rdbuf();
      if (!file) {
        return file_error("read");
      }

      auto maybe_rules = parse_rules(
          rules_stream.str(), name(environment::DD_SPAN_SAMPLING_RULES_FILE));
      if (auto *error = maybe_rules.if_error()) {
        std::string prefix;
        prefix += "With ";
        append(prefix, name(environment::DD_SPAN_SAMPLING_RULES_FILE));
        prefix += '=';
        append(prefix, *file_env);
        prefix += ": ";
        return error->with_prefix(prefix);
      }

      env_config.rules = std::move(*maybe_rules);
    }
  }

  return env_config;
}

}  // namespace

SpanSamplerConfig::Rule::Rule(const SpanMatcher &base) : SpanMatcher(base) {}

Expected<FinalizedSpanSamplerConfig> finalize_config(
    const SpanSamplerConfig &user_config, Logger &logger) {
  Expected<SpanSamplerConfig> env_config = load_span_sampler_env_config(logger);
  if (auto error = env_config.if_error()) {
    return *error;
  }

  FinalizedSpanSamplerConfig result;

  std::vector<SpanSamplerConfig::Rule> rules;
  if (!env_config->rules.empty()) {
    rules = env_config->rules;
    result.metadata[ConfigName::SPAN_SAMPLING_RULES] =
        ConfigMetadata(ConfigName::SPAN_SAMPLING_RULES, to_string(rules),
                       ConfigMetadata::Origin::ENVIRONMENT_VARIABLE);
  } else if (!user_config.rules.empty()) {
    rules = user_config.rules;
    result.metadata[ConfigName::SPAN_SAMPLING_RULES] =
        ConfigMetadata(ConfigName::SPAN_SAMPLING_RULES, to_string(rules),
                       ConfigMetadata::Origin::CODE);
  }

  for (const auto &rule : rules) {
    auto maybe_rate = Rate::from(rule.sample_rate);
    if (auto *error = maybe_rate.if_error()) {
      std::string prefix;
      prefix +=
          "Unable to parse sample_rate in span sampling rule with span "
          "pattern ";
      prefix += rule.to_json().dump();
      prefix += ": ";
      return error->with_prefix(prefix);
    }

    const auto allowed_types = {FP_NORMAL, FP_SUBNORMAL};
    if (rule.max_per_second &&
        (!(*rule.max_per_second > 0) ||
         std::find(std::begin(allowed_types), std::end(allowed_types),
                   std::fpclassify(*rule.max_per_second)) ==
             std::end(allowed_types))) {
      std::string message;
      message += "Span sampling rule with pattern ";
      message += rule.to_json().dump();
      message +=
          " should have a max_per_second value greater than zero, but the "
          "following value was given: ";
      message += std::to_string(*rule.max_per_second);
      return Error{Error::MAX_PER_SECOND_OUT_OF_RANGE, std::move(message)};
    }

    FinalizedSpanSamplerConfig::Rule finalized;
    static_cast<SpanMatcher &>(finalized) = rule;
    finalized.sample_rate = *maybe_rate;
    finalized.max_per_second = rule.max_per_second;
    result.rules.push_back(std::move(finalized));
  }

  return result;
}

nlohmann::json to_json(const FinalizedSpanSamplerConfig::Rule &rule) {
  // Get the base class's fields, then add our own.
  auto result = static_cast<const SpanMatcher &>(rule).to_json();
  result["sample_rate"] = double(rule.sample_rate);
  if (rule.max_per_second) {
    result["max_per_second"] = *rule.max_per_second;
  }

  return result;
}

}  // namespace tracing
}  // namespace datadog
