#include <datadog/null_collector.h>
#include <datadog/optional.h>
#include <datadog/platform_util.h>
#include <datadog/rate.h>
#include <datadog/tags.h>
#include <datadog/trace_segment.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <regex>
#include <vector>

#include "matchers.h"
#include "mocks/collectors.h"
#include "mocks/dict_readers.h"
#include "mocks/dict_writers.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

namespace {

Rate assert_rate(double rate) {
  // If `rate` is not valid, `std::variant` will throw an exception.
  return *Rate::from(rate);
}

}  // namespace

TEST_CASE("TraceSegment accessors") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  SECTION("hostname") {
    const bool report_hostname = GENERATE(true, false);

    config.report_hostname = report_hostname;
    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    auto span = tracer.create_span();

    auto hostname = span.trace_segment().hostname();
    if (report_hostname) {
      REQUIRE(hostname);
    } else {
      REQUIRE(!hostname);
    }
  }

  SECTION("defaults") {
    const std::unordered_map<std::string, std::string> tags{{"hello", "world"},
                                                            {"foo", "bar"}};

    config.name = "wobble";
    config.service_type = "fake";
    config.version = "v0";
    config.environment = "test";
    config.tags = tags;

    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    auto span = tracer.create_span();

    const auto span_default = span.trace_segment().defaults();
    CHECK(span_default.service == "testsvc");
    CHECK(span_default.name == "wobble");
    CHECK(span_default.service_type == "fake");
    CHECK(span_default.version == "v0");
    CHECK(span_default.environment == "test");
    CHECK(span_default.tags == tags);
  }

  SECTION("origin") {
    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};

    const std::unordered_map<std::string, std::string> headers{
        {"x-datadog-trace-id", "123"},
        {"x-datadog-parent-id", "456"},
        {"x-datadog-origin", "Unalaska"}};
    MockDictReader reader{headers};
    auto span = tracer.extract_span(reader);
    REQUIRE(span);
    REQUIRE(span->trace_segment().origin() == "Unalaska");
  }

  SECTION("sampling_decision") {
    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};

    SECTION("default create_span  →  no decision") {
      auto span = tracer.create_span();
      auto decision = span.trace_segment().sampling_decision();
      REQUIRE(!decision);
    }

    SECTION("after injecting at least once  →  local decision") {
      auto span = tracer.create_span();
      MockDictWriter writer;
      span.inject(writer);
      auto decision = span.trace_segment().sampling_decision();
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::LOCAL);
    }

    SECTION("extracted priority  →  extracted decision") {
      const std::unordered_map<std::string, std::string> headers{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-parent-id", "456"},
          {"x-datadog-sampling-priority", "7"}};  // 😯
      MockDictReader reader{headers};
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
      auto decision = span->trace_segment().sampling_decision();
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::EXTRACTED);
    }

    SECTION("override on segment  →  local decision") {
      auto span = tracer.create_span();
      span.trace_segment().override_sampling_priority(-10);  // 😵
      auto decision = span.trace_segment().sampling_decision();
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::LOCAL);
    }
  }

  SECTION("logger") {
    auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};
    auto span = tracer.create_span();
    REQUIRE(&span.trace_segment().logger() == config.logger.get());
  }
}

TEST_CASE("When Collector::send fails, TraceSegment logs the error.") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<FailureCollector>();
  config.collector = collector;
  const auto logger = std::make_shared<MockLogger>();
  config.logger = logger;

  auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};
  {
    // The only span, created and then destroyed, so that the `TraceSegment`
    // will `.send` it to the `Collector`, which will fail.
    auto span = tracer.create_span();
    (void)span;
  }
  REQUIRE(logger->error_count() == 1);
  REQUIRE(logger->first_error().code == collector->failure.code);
}

TEST_CASE("TraceSegment finalization of spans") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  SECTION("root span") {
    SECTION(
        "'inject_max_size' propagation error if X-Datadog-Tags oversized on "
        "inject") {
      auto finalized = finalize_config(config);
      REQUIRE(finalized);
      Tracer tracer{*finalized};

      // Make a very large X-Datadog-Tags value.
      std::string trace_tags_value = "foo=bar";
      for (int i = 0; i < 10'000; ++i) {
        trace_tags_value += ',';
        trace_tags_value += "_dd.p.";
        trace_tags_value += std::to_string(i);
        trace_tags_value += '=';
        trace_tags_value += std::to_string(2 * i);
      }

      const std::unordered_map<std::string, std::string> headers{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-parent-id", "456"},
          {"x-datadog-tags", trace_tags_value}};
      MockDictReader reader{headers};
      {
        auto span = tracer.extract_span(reader);
        REQUIRE(span);

        // Injecting the oversized X-Datadog-Tags will make `TraceSegment` note
        // an error, which it will later tag on the root span.
        MockDictWriter writer;
        span->inject(writer);
        REQUIRE(writer.items.count("x-datadog-tags") == 0);
      }

      REQUIRE(collector->first_span().tags.at(
                  tags::internal::propagation_error) == "inject_max_size");
    }

    SECTION("sampling priority") {
      auto finalized = finalize_config(config);
      REQUIRE(finalized);
      Tracer tracer{*finalized};

      SECTION("create trace -> priority in root span") {
        {
          auto root = tracer.create_span();
          (void)root;
        }
        REQUIRE(collector->span_count() == 1);
        const auto& span = collector->first_span();
        REQUIRE(span.numeric_tags.count(tags::internal::sampling_priority) ==
                1);
        // The value depends on the trace ID, so we won't check it here.
      }

      SECTION(
          "extracted sampling priority -> local root sampling priority same as "
          "extracted") {
        auto sampling_priority = GENERATE(-1, 0, 1, 2);
        const std::unordered_map<std::string, std::string> headers{
            {"x-datadog-trace-id", "123"},
            {"x-datadog-parent-id", "456"},
            {"x-datadog-sampling-priority", std::to_string(sampling_priority)},
        };
        MockDictReader reader{headers};
        { auto span = tracer.extract_span(reader); }
        REQUIRE(collector->span_count() == 1);
        REQUIRE(collector->first_span().numeric_tags.at(
                    tags::internal::sampling_priority) == sampling_priority);
      }

      SECTION(
          "override sampling priority  -> local root sampling priority same as "
          "override") {
        auto sampling_priority = GENERATE(-1, 0, 1, 2);
        {
          auto root = tracer.create_span();
          root.trace_segment().override_sampling_priority(sampling_priority);
        }
        REQUIRE(collector->span_count() == 1);
        REQUIRE(collector->first_span().numeric_tags.at(
                    tags::internal::sampling_priority) == sampling_priority);
      }

      SECTION(
          "inject span -> injected priority is the same as that sent to agent "
          "in local root span") {
        MockDictWriter writer;
        {
          auto root = tracer.create_span();
          root.inject(writer);
        }
        REQUIRE(collector->span_count() == 1);
        REQUIRE(std::to_string(int(collector->first_span().numeric_tags.at(
                    tags::internal::sampling_priority))) ==
                writer.items.at("x-datadog-sampling-priority"));
      }
    }

    SECTION("hostname") {
      config.report_hostname = true;
      auto finalized = finalize_config(config);
      REQUIRE(finalized);
      Tracer tracer{*finalized};
      {
        auto root = tracer.create_span();
        (void)root;
      }
      REQUIRE(collector->span_count() == 1);
      REQUIRE(collector->first_span().tags.at(tags::internal::hostname) ==
              get_hostname());
    }

    SECTION("x-datadog-tags") {
      auto finalized = finalize_config(config);
      REQUIRE(finalized);
      Tracer tracer{*finalized};

      const std::unordered_map<std::string, std::string> headers{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-parent-id", "456"},
          {"x-datadog-tags", "_dd.p.one=1,_dd.p.two=2,three=3"},
      };
      MockDictReader reader{headers};
      {
        auto span = tracer.extract_span(reader);
        (void)span;
      }

      const std::unordered_map<std::string, std::string> filtered{
          {"_dd.p.one", "1"}, {"_dd.p.two", "2"}};

      REQUIRE(collector->span_count() == 1);
      const auto& span = collector->first_span();
      // "three" will be discarded, but not the other two.
      REQUIRE(span.tags.count("three") == 0);
      REQUIRE_THAT(span.tags, ContainsSubset(filtered));
      // "_dd.p.dm" will be added, because we made a sampling decision.
      REQUIRE(span.tags.count("_dd.p.dm") == 1);
    }

    SECTION("rate tags") {
      SECTION("default mechanism (100%) -> agent psr tag on first trace") {
        auto finalized = finalize_config(config);
        REQUIRE(finalized);
        Tracer tracer{*finalized};
        {
          auto span = tracer.create_span();
          (void)span;
        }
        REQUIRE(collector->span_count() == 1);
        const auto& span = collector->first_span();
        REQUIRE(span.numeric_tags.at(tags::internal::agent_sample_rate) == 1.0);
      }

      SECTION(
          "agent catch-all response @100% -> agent psr tag on second trace") {
        const auto collector = std::make_shared<MockCollectorWithResponse>();
        collector->response
            .sample_rate_by_key[CollectorResponse::key_of_default_rate] =
            assert_rate(1.0);
        config.collector = collector;

        auto finalized = finalize_config(config);
        REQUIRE(finalized);
        Tracer tracer{*finalized};
        // First trace doesn't have a collector-specified sample rate.
        {
          auto span = tracer.create_span();
          (void)span;
        }
        REQUIRE(collector->span_count() == 1);

        collector->chunks.clear();
        // Second trace will use the rate from `collector->response`.
        {
          auto span = tracer.create_span();
          (void)span;
        }
        REQUIRE(collector->span_count() == 1);
        const auto& span = collector->first_span();
        REQUIRE(span.numeric_tags.at(tags::internal::agent_sample_rate) == 1.0);
      }

      SECTION("rules (implicit and explicit)") {
        // When sample rate is 100%, the sampler will consult the limiter.
        // When sample rate is 0%, it won't.  We test both cases.
        auto sample_rate = GENERATE(0.0, 1.0);

        SECTION("global sample rate") {
          config.trace_sampler.sample_rate = sample_rate;
        }

        SECTION("sampling rule") {
          TraceSamplerConfig::Rule rule;
          rule.service = "testsvc";
          rule.sample_rate = sample_rate;
          config.trace_sampler.rules.push_back(rule);
        }

        auto finalized = finalize_config(config);
        REQUIRE(finalized);
        Tracer tracer{*finalized};
        {
          auto span = tracer.create_span();
          (void)span;
        }
        REQUIRE(collector->span_count() == 1);
        const auto& span = collector->first_span();
        REQUIRE(span.numeric_tags.at(tags::internal::rule_sample_rate) ==
                sample_rate);
        if (sample_rate == 1.0) {
          REQUIRE(span.numeric_tags.at(
                      tags::internal::rule_limiter_sample_rate) == 1.0);
        } else {
          REQUIRE(sample_rate == 0.0);
          REQUIRE(span.numeric_tags.count(
                      tags::internal::rule_limiter_sample_rate) == 0);
        }
      }
    }
  }  // root span

  SECTION(
      "every span tagged with: _dd.origin, process_id, language, resource-id") {
    const auto finalized = finalize_config(config);
    REQUIRE(finalized);
    Tracer tracer{*finalized};

    std::unordered_map<std::string, std::string> headers;
    headers["x-datadog-trace-id"] = "123";
    headers["x-datadog-parent-id"] = "456";
    headers["x-datadog-origin"] = "พัทยา";
    MockDictReader reader{headers};
    auto maybe_span = tracer.extract_span(reader);
    REQUIRE(maybe_span);
    {
      std::vector<Span> spans;
      spans.push_back(std::move(*maybe_span));
      // Create some descendants.
      for (int i = 0; i < 10; ++i) {
        spans.push_back(spans.front().create_child());
        spans.push_back(spans.back().create_child());
      }
    }

    const int process_id = get_process_id();
    // clang-format off
    const char uuid_pattern[] =
        "[0-9a-f]{8}"
        "-"
        "[0-9a-f]{4}"
        "-"
        "[0-9a-f]{4}"
        "-"
        "[0-9a-f]{4}"
        "-"
        "[0-9a-f]{12}";
    // clang-format on
    const std::regex uuid_regex{uuid_pattern};

    REQUIRE(collector->span_count() == 2 * 10 + 1);
    for (const auto& chunk : collector->chunks) {
      for (const auto& span : chunk) {
        REQUIRE(span);

        auto found_string = span->tags.find(tags::internal::origin);
        REQUIRE(found_string != span->tags.end());
        REQUIRE(found_string->second == "พัทยา");

        found_string = span->tags.find(tags::internal::language);
        REQUIRE(found_string != span->tags.end());
        REQUIRE(found_string->second == "cpp");

        found_string = span->tags.find(tags::internal::runtime_id);
        REQUIRE(found_string != span->tags.end());
        const auto found_uuid = found_string->second;
        CAPTURE(found_uuid);
        REQUIRE(std::regex_match(found_uuid, uuid_regex));

        const auto found_number =
            span->numeric_tags.find(tags::internal::process_id);
        REQUIRE(found_number != span->numeric_tags.end());
        REQUIRE(found_number->second == process_id);
      }
    }
  }
}  // span finalizers

TEST_CASE("independent of Tracer") {
  // This test verifies that a `TraceSegment` (via the `Span`s that refer to it)
  // can continue to operate even after the `Tracer` that created it is
  // destroyed.
  //
  // Primarily, the test checks that the code doesn't crash in this scenario.
  TracerConfig config;
  config.service = "testsvc";
  config.name = "do.thing";
  config.collector = std::make_shared<NullCollector>();
  config.logger = std::make_shared<NullLogger>();

  auto maybe_tracer = finalize_config(config);
  REQUIRE(maybe_tracer);
  Optional<Tracer> tracer{*maybe_tracer};

  Span root = tracer->create_span();
  Span child = root.create_child();

  tracer.reset();
}
