#include <datadog/optional.h>
#include <datadog/span_data.h>
#include <datadog/span_sampler.h>
#include <datadog/tags.h>
#include <datadog/tracer.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "mocks/collectors.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

namespace std {

std::ostream& operator<<(std::ostream& stream, Optional<double> value) {
  if (value) {
    return stream << *value;
  }
  return stream << "null";
}

std::ostream& operator<<(
    std::ostream& stream,
    const std::unordered_map<std::string, double>& numeric_tags) {
  stream << "{";
  auto iter = numeric_tags.begin();
  const auto end = numeric_tags.end();
  if (iter != end) {
    stream << '\"' << iter->first << "\": " << iter->second;
    for (++iter; iter != end; ++iter) {
      stream << ", ";
      stream << '\"' << iter->first << "\": " << iter->second;
    }
  }
  return stream << "}";
}

}  // namespace std

namespace {

struct SpanSamplingTags {
  Optional<double> mechanism;
  Optional<double> rule_rate;
  Optional<double> max_per_second;
};

SpanSamplingTags span_sampling_tags(const SpanData& span) {
  SpanSamplingTags result;

  auto found = span.numeric_tags.find(tags::internal::span_sampling_mechanism);
  if (found != span.numeric_tags.end()) {
    result.mechanism = found->second;
  }
  found = span.numeric_tags.find(tags::internal::span_sampling_rule_rate);
  if (found != span.numeric_tags.end()) {
    result.rule_rate = found->second;
  }
  found = span.numeric_tags.find(tags::internal::span_sampling_limit);
  if (found != span.numeric_tags.end()) {
    result.max_per_second = found->second;
  }

  return result;
}

SpanSamplerConfig::Rule by_service(StringView service) {
  SpanSamplerConfig::Rule rule;
  rule.service = service;
  return rule;
}

SpanSamplerConfig::Rule by_name(StringView name) {
  SpanSamplerConfig::Rule rule;
  rule.name = name;
  return rule;
}

SpanSamplerConfig::Rule by_resource(StringView resource) {
  SpanSamplerConfig::Rule rule;
  rule.resource = resource;
  return rule;
}

SpanSamplerConfig::Rule by_tags(
    std::unordered_map<std::string, std::string> tags) {
  SpanSamplerConfig::Rule rule;
  rule.tags = std::move(tags);
  return rule;
}

SpanSamplerConfig::Rule by_name_and_tags(
    StringView name, std::unordered_map<std::string, std::string> tags) {
  SpanSamplerConfig::Rule rule;
  rule.name = name;
  rule.tags = std::move(tags);
  return rule;
}

const auto x = nullopt;

}  // namespace

TEST_CASE("span rules matching") {
  struct TestCase {
    std::string name;
    std::vector<SpanSamplerConfig::Rule> rules;
    SpanSamplingTags expected_parent;
    SpanSamplingTags expected_child;
    SpanSamplingTags expected_sibling;
    SpanSamplingTags expected_grandchild;
  };

  auto test_case = GENERATE(values<TestCase>({
      {"no rules → no span sampling tags", {}, {}, {}, {}, {}},
      {"match by service",
       {by_service("testsvc")},
       {8, 1.0, x},
       {8, 1.0, x},
       {8, 1.0, x},
       {8, 1.0, x}},
      {"match by name",
       {by_name("sibling")},
       {x, x, x},
       {x, x, x},
       {8, 1.0, x},
       {x, x, x}},
      {"match by resource",
       {by_resource("office")},
       {x, x, x},
       {8, 1.0, x},
       {x, x, x},
       {x, x, x}},
      {"match by tag",
       {by_tags({{"generation", "second"}})},
       {x, x, x},
       {8, 1.0, x},
       {8, 1.0, x},
       {x, x, x}},
      {"match by name and tag",
       {by_name_and_tags("child", {{"generation", "second"}})},
       {x, x, x},
       {8, 1.0, x},
       {x, x, x},
       {x, x, x}},
  }));

  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();
  config.span_sampler.rules = test_case.rules;
  config.trace_sampler.sample_rate = 0;  // drop the trace

  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};
  {
    auto parent = tracer.create_span();
    parent.set_name("parent");
    parent.set_resource_name("factory");
    parent.set_tag("generation", "first");
    auto child = parent.create_child();
    child.set_name("child");
    child.set_resource_name("office");
    child.set_tag("generation", "second");
    auto sibling = parent.create_child();
    sibling.set_name("sibling");
    sibling.set_resource_name("prison");
    sibling.set_tag("generation", "second");
    auto grandchild = child.create_child();
    grandchild.set_name("grandchild");
    grandchild.set_resource_name("studio");
    grandchild.set_tag("generation", "third");
    grandchild.set_tag("youngest", "");
  }

  CAPTURE(test_case.name);
  REQUIRE(collector->chunks.size() == 1);
  for (const auto& span_ptr : collector->chunks.front()) {
    const auto& span = *span_ptr;
    CAPTURE(span.numeric_tags);
    const auto tags = span_sampling_tags(span);
    SpanSamplingTags expected_tags;
    if (span.name == "parent") {
      expected_tags = test_case.expected_parent;
    } else if (span.name == "child") {
      expected_tags = test_case.expected_child;
    } else if (span.name == "sibling") {
      expected_tags = test_case.expected_sibling;
    } else {
      REQUIRE(span.name == "grandchild");
      expected_tags = test_case.expected_grandchild;
    }

    REQUIRE(expected_tags.mechanism == tags.mechanism);
    REQUIRE(expected_tags.rule_rate == tags.rule_rate);
    REQUIRE(expected_tags.max_per_second == tags.max_per_second);
  }
}

TEST_CASE("span rules only on trace drop") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();
  config.span_sampler.rules.push_back(by_service("testsvc"));

  struct TestCase {
    std::string name;
    enum { KEEP_TRACE, DROP_TRACE } decision;
    SpanSamplingTags expected_tags;
  };

  auto test_case = GENERATE(values<TestCase>({
      {"trace drop → span sampling tags", TestCase::DROP_TRACE, {8, 1.0, x}},
      {"trace keep →  no span sampling tags", TestCase::KEEP_TRACE, {x, x, x}},
  }));

  CAPTURE(test_case.name);
  config.trace_sampler.sample_rate =
      test_case.decision == TestCase::KEEP_TRACE ? 1.0 : 0.0;
  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};
  {
    auto span = tracer.create_span();
    (void)span;
  }

  const auto tags = span_sampling_tags(collector->first_span());
  REQUIRE(test_case.expected_tags.mechanism == tags.mechanism);
  REQUIRE(test_case.expected_tags.rule_rate == tags.rule_rate);
  REQUIRE(test_case.expected_tags.max_per_second == tags.max_per_second);
}

TEST_CASE("span rule sample rate") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  struct TestCase {
    std::string name;
    double span_rule_rate;
    SpanSamplingTags expected_tags;
  };

  auto test_case = GENERATE(values<TestCase>({
      {"100% → span sampling tags", 1.0, {8, 1.0, x}},
      {"0% →  no span sampling tags", 0.0, {x, x, x}},
  }));

  CAPTURE(test_case.name);

  SpanSamplerConfig::Rule rule;
  rule.service = "testsvc";
  rule.sample_rate = test_case.span_rule_rate;
  config.span_sampler.rules.push_back(rule);
  config.trace_sampler.sample_rate = 0.0;  // drop the trace
  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};
  {
    auto span = tracer.create_span();
    (void)span;
  }

  const auto tags = span_sampling_tags(collector->first_span());
  REQUIRE(test_case.expected_tags.mechanism == tags.mechanism);
  REQUIRE(test_case.expected_tags.rule_rate == tags.rule_rate);
  REQUIRE(test_case.expected_tags.max_per_second == tags.max_per_second);
}

TEST_CASE("span rule limiter") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();
  config.trace_sampler.sample_rate = 0.0;  // drop the trace

  struct TestCase {
    std::string name;
    std::size_t num_spans;
    Optional<double> max_per_second;
    std::size_t expected_count;
  };

  auto test_case =
      GENERATE(values<TestCase>({{"default is no limit", 1000, x, 1000},
                                 {"limiter limits", 1000, 100, 100}}));

  CAPTURE(test_case.name);
  SpanSamplerConfig::Rule rule;
  rule.max_per_second = test_case.max_per_second;
  config.span_sampler.rules.push_back(rule);

  auto clock = [frozen_time = default_clock()]() { return frozen_time; };
  auto finalized = finalize_config(config, clock);
  REQUIRE(finalized);
  Tracer tracer{*finalized};

  for (std::size_t i = 0; i < test_case.num_spans; ++i) {
    auto span = tracer.create_span();
    (void)span;
  }

  // Because of the way we created spans above, each will be in its own chunk.
  std::size_t count_of_sampled_spans = 0;
  for (const auto& chunk : collector->chunks) {
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;
    if (span_sampling_tags(span).mechanism) {
      ++count_of_sampled_spans;
    }
  }

  REQUIRE(count_of_sampled_spans == test_case.expected_count);
}
