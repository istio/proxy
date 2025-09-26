#include <datadog/clock.h>
#include <datadog/id_generator.h>
#include <datadog/rate.h>
#include <datadog/sampling_priority.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <chrono>
#include <limits>
#include <map>
#include <ostream>

#include "mocks/collectors.h"
#include "mocks/loggers.h"
#include "test.h"

namespace std {

// This is for printing the mapping between sampling priority and trace count
// when a test below fails.
std::ostream& operator<<(std::ostream& stream,
                         const std::map<int, std::size_t>& counts) {
  stream << "{";
  auto iter = counts.begin();
  if (iter != counts.end()) {
    stream << '\"' << iter->first << "\": " << iter->second;
    for (++iter; iter != counts.end(); ++iter) {
      stream << ", \"" << iter->first << "\": " << iter->second;
    }
  }
  return stream << "}";
}

}  // namespace std

namespace {

Rate assert_rate(double rate) {
  // If `rate` is not valid, `std::variant` will throw an exception.
  return *Rate::from(rate);
}

}  // namespace

using namespace datadog::tracing;

TEST_CASE("trace sampling rule sample rate") {
  // For a configured global sample rate, verify that the average proportion of
  // traces kept matches the rate.
  struct TestCase {
    std::string name;
    double sample_rate;
  };

  auto test_case = GENERATE(values<TestCase>({{"drop all", 0.0},
                                              {"keep all", 1.0},
                                              {"half", 0.5},
                                              {"keep few", 0.01},
                                              {"keep most", 0.99}}));

  CAPTURE(test_case.name);
  CAPTURE(test_case.sample_rate);

  const std::size_t num_iterations = 10'000;
  TracerConfig config;
  config.service = "testsvc";
  config.trace_sampler.sample_rate = test_case.sample_rate;
  // Plenty of head room so that the limiter doesn't throttle us.
  config.trace_sampler.max_per_second = num_iterations * 2;
  const auto collector = std::make_shared<PriorityCountingCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};

  for (std::size_t i = 0; i < num_iterations; ++i) {
    auto span = tracer.create_span();
    (void)span;
  }

  auto& priority_counts = collector->sampling_priority_count;
  CAPTURE(priority_counts);

  // Some of the traces will have priority -1 ("user drop") and others will have
  // priority 2 ("user keep"), but no other values.
  REQUIRE(priority_counts.size() <= 2);
  // I assume that there have been enough trials that not _all_ traces are kept
  // or dropped purely due to chance.  That could happen only if the sample rate
  // were 0% or 100%, respectively.
  REQUIRE((test_case.sample_rate == 0.0 ||
           priority_counts.count(int(SamplingPriority::USER_KEEP))));
  REQUIRE((test_case.sample_rate == 1.0 ||
           priority_counts.count(int(SamplingPriority::USER_DROP))));

  REQUIRE(collector->total_count() == num_iterations);

  const double rate_kept = collector->ratio_of(SamplingPriority::USER_KEEP);
  REQUIRE(rate_kept == Approx(test_case.sample_rate).margin(0.05));
}

TEST_CASE("trace sampling rate limiter") {
  // Verify that the average proportion of traces kept over the course of a
  // second does not exceed that allowed by the configured limit.
  struct TestCase {
    std::string name;
    double max_per_second;
    std::size_t burst_size;
    std::size_t expected_kept_count;
  };

  auto test_case = GENERATE(values<TestCase>({{"allow one", 1.0, 100, 1},
                                              {"allow all", 100.0, 100, 100},
                                              {"allow some", 10.0, 100, 10}}));

  CAPTURE(test_case.name);
  CAPTURE(test_case.max_per_second);
  CAPTURE(test_case.burst_size);
  CAPTURE(test_case.expected_kept_count);

  TracerConfig config;
  config.service = "testsvc";
  config.trace_sampler.sample_rate = 1.0;
  config.trace_sampler.max_per_second = test_case.max_per_second;
  const auto collector = std::make_shared<PriorityCountingCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  TimePoint current_time = default_clock();
  // Modify `current_time` to advance the clock.
  auto clock = [&current_time]() { return current_time; };

  auto finalized = finalize_config(config, clock);
  REQUIRE(finalized);

  Tracer tracer{*finalized};

  for (std::size_t i = 0; i < test_case.burst_size; ++i) {
    auto span = tracer.create_span();
    (void)span;
  }

  REQUIRE(collector->total_count() == test_case.burst_size);
  REQUIRE(collector->count_of(SamplingPriority::USER_KEEP) ==
          test_case.expected_kept_count);

  // Now verify that there is a "cooldown period" of one second, after which
  // the limiter will permit some more traces.  How many it permits depends
  // on how "over budget" it was, but it will allow at least one.
  collector->sampling_priority_count.clear();
  current_time += std::chrono::seconds(1);
  {
    auto span = tracer.create_span();
    (void)span;
  }
  REQUIRE(collector->count_of(SamplingPriority::USER_KEEP) == 1);
}

TEST_CASE("priority sampling") {
  // Verify that a `TraceSampler` not otherwise configured will use whichever
  // sample rates are sent back to it by the collector (Datadog Agent).
  const std::size_t num_iterations = 10'000;

  struct TestCase {
    std::string name;
    std::string service_key;
    double sample_rate;
    double expected_rate;
  };

  auto test_case = GENERATE(values<TestCase>(
      {{"default rate", CollectorResponse::key_of_default_rate, 0.5, 0.5},
       {"testsvc on dev", "service:testsvc,env:dev", 0.5, 0.5},
       {"no match uses default of 100%", "service:unrelated,env:foo", 0.25,
        1.0}}));

  TracerConfig config;
  config.service = "testsvc";
  config.environment = "dev";
  // plenty of head room
  config.trace_sampler.max_per_second = 2 * num_iterations;
  const auto collector =
      std::make_shared<PriorityCountingCollectorWithResponse>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};

  collector->response.sample_rate_by_key[test_case.service_key] =
      assert_rate(test_case.sample_rate);

  for (std::size_t i = 0; i < num_iterations; ++i) {
    auto span = tracer.create_span();
    (void)span;
  }

  REQUIRE(collector->total_count() == num_iterations);

  // Priority sampling uses sampling priority 1 ("auto keep").
  REQUIRE(collector->ratio_of(SamplingPriority::AUTO_KEEP) ==
          Approx(test_case.expected_rate).margin(0.05));
}

TEST_CASE("sampling rules") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<PriorityCountingCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  SECTION("no rule matches → priority sampling") {
    TraceSamplerConfig::Rule rule;
    rule.service = "foosvc";
    config.trace_sampler.rules.push_back(rule);

    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    {
      auto span = tracer.create_span();
      (void)span;
    }

    REQUIRE(collector->total_count() == 1);
    REQUIRE(collector->count_of(SamplingPriority::AUTO_KEEP) +
                collector->count_of(SamplingPriority::AUTO_DROP) ==
            1);
  }

  SECTION("matches first rule") {
    TraceSamplerConfig::Rule rule;
    rule.service = "testsvc";
    rule.sample_rate = 1.0;  // this is also the default
    config.trace_sampler.rules.push_back(rule);

    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    {
      auto span = tracer.create_span();
      (void)span;
    }

    REQUIRE(collector->total_count() == 1);
    REQUIRE(collector->count_of(SamplingPriority::USER_KEEP) == 1);
  }

  SECTION("matches second rule") {
    TraceSamplerConfig::Rule rule;
    rule.service = "foosvc";
    rule.sample_rate = 1.0;  // this is also the default
    config.trace_sampler.rules.push_back(rule);

    rule.service = "testsvc";
    rule.sample_rate = 0.0;
    config.trace_sampler.rules.push_back(rule);

    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    {
      auto span = tracer.create_span();
      (void)span;
    }

    REQUIRE(collector->total_count() == 1);
    REQUIRE(collector->count_of(SamplingPriority::USER_DROP) == 1);
  }
}
