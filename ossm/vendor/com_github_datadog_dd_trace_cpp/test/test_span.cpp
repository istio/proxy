// These are tests for `Span`.  `Span` is a container for labels associated with
// an extent in time.  `Span` is also responsible for injecting trace context
// for propagation.

#include <datadog/clock.h>
#include <datadog/hex.h>
#include <datadog/injection_options.h>
#include <datadog/null_collector.h>
#include <datadog/optional.h>
#include <datadog/span.h>
#include <datadog/span_config.h>
#include <datadog/tag_propagation.h>
#include <datadog/trace_segment.h>
#include <datadog/tracer.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "catch.hpp"
#include "datadog/sampling_mechanism.h"
#include "matchers.h"
#include "mocks/collectors.h"
#include "mocks/dict_readers.h"
#include "mocks/dict_writers.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

TEST_CASE("set_tag") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("tags end up in the collector") {
    {
      auto span = tracer.create_span();
      span.set_tag("foo", "lemon");
      span.set_tag("foo.bar", "mint");
      span.set_tag("foo.baz", "blueberry");
      span.set_tag("_dd.secret.sauce", "thousand islands");
      span.set_tag("_dd_not_internal", "");
      span.set_tag("_dd.chipmunk", "");
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;
    REQUIRE(span.tags.at("foo") == "lemon");
    REQUIRE(span.tags.at("foo.bar") == "mint");
    REQUIRE(span.tags.at("foo.baz") == "blueberry");
    REQUIRE(span.tags.at("_dd.secret.sauce") == "thousand islands");
    REQUIRE(span.tags.at("_dd_not_internal") == "");
    REQUIRE(span.tags.at("_dd.chipmunk") == "");
  }

  SECTION("tags can be overwritten") {
    {
      SpanConfig config;
      config.tags = {{"color", "purple"},
                     {"turtle.depth", "all the way down"},
                     {"_dd.tag", "written"}};
      auto span = tracer.create_span(config);
      span.set_tag("color", "green");
      span.set_tag("bonus", "applied");
      span.set_tag("_dd.tag", "overwritten");
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;
    REQUIRE(span.tags.at("color") == "green");
    REQUIRE(span.tags.at("turtle.depth") == "all the way down");
    REQUIRE(span.tags.at("bonus") == "applied");
    REQUIRE(span.tags.at("_dd.tag") == "overwritten");
  }
}

TEST_CASE("lookup_tag") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("not found is null") {
    auto span = tracer.create_span();
    REQUIRE(!span.lookup_tag("nope"));
    REQUIRE(!span.lookup_tag("also nope"));
    REQUIRE(!span.lookup_tag("_dd.nope"));
  }

  SECTION("lookup after set") {
    auto span = tracer.create_span();
    span.set_tag("color", "purple");
    span.set_tag("turtle.depth", "all the way down");
    span.set_tag("_dd.tag", "found");

    REQUIRE(span.lookup_tag("color") == "purple");
    REQUIRE(span.lookup_tag("turtle.depth") == "all the way down");
    REQUIRE(span.lookup_tag("_dd.tag") == "found");
  }

  SECTION("lookup after config") {
    SpanConfig config;
    config.tags = {
        {"color", "purple"},
        {"turtle.depth", "all the way down"},
        {"_dd.tag", "found"},
    };
    auto span = tracer.create_span(config);

    REQUIRE(span.lookup_tag("color") == "purple");
    REQUIRE(span.lookup_tag("turtle.depth") == "all the way down");
    REQUIRE(span.lookup_tag("_dd.tag") == "found");
  }
}

TEST_CASE("remove_tag") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("doesn't have to be there already") {
    auto span = tracer.create_span();
    span.remove_tag("not even there");
    span.remove_tag("_dd.tag");
  }

  SECTION("after removal, lookup yields null") {
    SpanConfig config;
    config.tags = {{"mayfly", "carpe diem"}, {"_dd.mayfly", "carpe diem"}};
    auto span = tracer.create_span(config);
    span.set_tag("foo", "bar");

    span.remove_tag("mayfly");
    span.remove_tag("_dd.mayfly");
    span.remove_tag("foo");

    REQUIRE(!span.lookup_tag("mayfly"));
    REQUIRE(!span.lookup_tag("_dd.mayfly"));
    REQUIRE(!span.lookup_tag("foo"));
  }
}

TEST_CASE("set_metric") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("metrics end up in the collector") {
    {
      auto span = tracer.create_span();
      span.set_metric("foo", 5.0);
      span.set_metric("foo.bar", 3.0);
      span.set_metric("foo.baz", 1.0);
      span.set_metric("_dd.secret.sauce", 2.0);
      span.set_metric("_dd_not_internal", 3.0);
      span.set_metric("_dd.chipmunk", 4.0);
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;
    REQUIRE(span.numeric_tags.at("foo") == 5.0);
    REQUIRE(span.numeric_tags.at("foo.bar") == 3.0);
    REQUIRE(span.numeric_tags.at("foo.baz") == 1.0);
    REQUIRE(span.numeric_tags.at("_dd.secret.sauce") == 2.0);
    REQUIRE(span.numeric_tags.at("_dd_not_internal") == 3.0);
    REQUIRE(span.numeric_tags.at("_dd.chipmunk") == 4.0);
  }

  SECTION("metrics can be overwritten") {
    {
      auto span = tracer.create_span();
      span.set_metric("color", 2.0);
      span.set_metric("color", 1.0);
      span.set_metric("bonus", 6.0);
      span.set_metric("bonus", 5.0);
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;
    REQUIRE(span.numeric_tags.at("color") == 1.0);
    REQUIRE(span.numeric_tags.at("bonus") == 5.0);
  }
}

TEST_CASE("lookup_metric") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("not found is null") {
    auto span = tracer.create_span();
    REQUIRE(!span.lookup_metric("nope"));
    REQUIRE(!span.lookup_metric("also nope"));
    REQUIRE(!span.lookup_metric("_dd.nope"));
  }

  SECTION("lookup after set") {
    auto span = tracer.create_span();
    span.set_metric("color", 11.0);
    span.set_metric("turtle.depth", 6.0);
    span.set_metric("_dd.this", 33.0);

    REQUIRE(span.lookup_metric("color") == 11.0);
    REQUIRE(span.lookup_metric("turtle.depth") == 6.0);
    REQUIRE(span.lookup_metric("_dd.this") == 33.0);
  }
}

TEST_CASE("remove_metric") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("doesn't have to be there already") {
    auto span = tracer.create_span();
    span.remove_metric("not even there");
  }

  SECTION("after removal, lookup yields null") {
    auto span = tracer.create_span();
    span.set_metric("mayfly", 10.0);
    span.set_metric("foo", 11.0);
    span.set_metric("_dd.metric", 1.0);

    span.remove_metric("mayfly");
    span.remove_metric("foo");
    span.remove_metric("_dd.metric");

    REQUIRE(!span.lookup_metric("mayfly"));
    REQUIRE(!span.lookup_metric("foo"));
    REQUIRE(!span.lookup_metric("_dd.metric"));
  }
}

TEST_CASE("span duration") {
  TracerConfig config;
  config.service = "testsvc";
  auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("start time is adjustable") {
    {
      SpanConfig config;
      config.start = default_clock() - std::chrono::seconds(3);
      auto span = tracer.create_span(config);
      (void)span;
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;

    REQUIRE(span.duration >= std::chrono::seconds(3));
  }

  SECTION("end time is adjustable") {
    {
      auto span = tracer.create_span();
      span.set_end_time(span.start_time().tick + std::chrono::seconds(2));
    }

    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;

    REQUIRE(span.duration == std::chrono::seconds(2));
  }
}

TEST_CASE(".error() and .set_error*()") {
  struct TestCase {
    std::string name;
    std::function<void(Span&)> mutate;
    bool expected_error;
    Optional<StringView> expected_error_message;
    Optional<StringView> expected_error_type;
    Optional<StringView> expected_error_stack;
  };

  auto test_case = GENERATE(values<TestCase>(
      {{"No error → no error.", [](Span&) {}, false, nullopt, nullopt, nullopt},
       {"set_error(true) → error", [](Span& span) { span.set_error(true); },
        true, nullopt, nullopt, nullopt},
       {"set_error_message → error and error message",
        [](Span& span) { span.set_error_message("oops!"); }, true, "oops!",
        nullopt, nullopt},
       {"set_error_type → error and error type",
        [](Span& span) { span.set_error_type("errno"); }, true, nullopt,
        "errno", nullopt},
       {"set_error_stack → error and error stack",
        [](Span& span) { span.set_error_stack("this is C++, fool"); }, true,
        nullopt, nullopt, "this is C++, fool"},
       {"set all of them → error, error message, error type, and error stack",
        [](Span& span) {
          span.set_error_message("oops!");
          span.set_error_type("errno");
          span.set_error_stack("this is C++, fool");
        },
        true, "oops!", "errno", "this is C++, fool"},
       {"set_error(false) → no error, no error tags, and no error stack",
        [](Span& span) {
          span.set_error_message("this will go away");
          span.set_error_type("as will this");
          span.set_error_stack("this too");
          span.set_error(false);
        },
        false, nullopt, nullopt, nullopt}}));

  TracerConfig config;
  config.service = "testsvc";
  auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  CAPTURE(test_case.name);
  {
    auto span = tracer.create_span();
    test_case.mutate(span);
    REQUIRE(span.error() == test_case.expected_error);
  }

  REQUIRE(collector->chunks.size() == 1);
  const auto& chunk = collector->chunks.front();
  REQUIRE(chunk.size() == 1);
  const auto& span_ptr = chunk.front();
  REQUIRE(span_ptr);
  const auto& span = *span_ptr;

  auto found = span.tags.find("error.message");
  if (test_case.expected_error_message) {
    REQUIRE(found != span.tags.end());
    REQUIRE(found->second == *test_case.expected_error_message);
  } else {
    REQUIRE(found == span.tags.end());
  }

  found = span.tags.find("error.type");
  if (test_case.expected_error_type) {
    REQUIRE(found != span.tags.end());
    REQUIRE(found->second == *test_case.expected_error_type);
  } else {
    REQUIRE(found == span.tags.end());
  }
}

TEST_CASE("property setters and getters") {
  // Verify that modifications made by `Span::set_...` are visible both in the
  // corresponding getter method and in the resulting span data sent to the
  // collector.
  TracerConfig config;
  config.service = "testsvc";
  auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<MockLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  SECTION("set_service_name") {
    {
      auto span = tracer.create_span();
      span.set_service_name("wobble");
      REQUIRE(span.service_name() == "wobble");
    }
    auto& span = collector->first_span();
    REQUIRE(span.service == "wobble");
  }

  SECTION("set_service_type") {
    {
      auto span = tracer.create_span();
      span.set_service_type("wobble");
      REQUIRE(span.service_type() == "wobble");
    }
    auto& span = collector->first_span();
    REQUIRE(span.service_type == "wobble");
  }

  SECTION("set_name") {
    {
      auto span = tracer.create_span();
      span.set_name("wobble");
      REQUIRE(span.name() == "wobble");
    }
    auto& span = collector->first_span();
    REQUIRE(span.name == "wobble");
  }

  SECTION("set_resource_name") {
    {
      auto span = tracer.create_span();
      span.set_resource_name("wobble");
      REQUIRE(span.resource_name() == "wobble");
    }
    auto& span = collector->first_span();
    REQUIRE(span.resource == "wobble");
  }
}

// Trace context injection is implemented in `TraceSegment`, but it's part of
// the interface of `Span`, so the test is here.
TEST_CASE("injection") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();
  config.injection_styles = {PropagationStyle::DATADOG, PropagationStyle::B3};

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  // Override the tracer's ID generator to always return a fixed value.
  struct Generator : public IDGenerator {
    const std::uint64_t id;
    explicit Generator(std::uint64_t id) : id(id) {}
    TraceID trace_id(const TimePoint&) const override { return TraceID(id); }
    std::uint64_t span_id() const override { return id; }
  };
  Tracer tracer{*finalized_config, std::make_shared<Generator>(42)};

  SECTION("trace ID, parent ID ,and sampling priority") {
    auto span = tracer.create_span();
    REQUIRE(span.trace_id() == 42);
    REQUIRE(span.id() == 42);

    const int priority = 3;  // 😱
    span.trace_segment().override_sampling_priority(priority);
    MockDictWriter writer;
    span.inject(writer);

    const auto& headers = writer.items;
    REQUIRE(headers.at("x-datadog-trace-id") == "42");
    REQUIRE(headers.at("x-datadog-parent-id") == "42");
    REQUIRE(headers.at("x-datadog-sampling-priority") == "3");
    REQUIRE(headers.count("x-datadog-delegate-trace-sampling") == 0);
    REQUIRE(headers.at("x-b3-traceid") == "000000000000002a");
    REQUIRE(headers.at("x-b3-spanid") == "000000000000002a");
    REQUIRE(headers.at("x-b3-sampled") == "1");
  }

  SECTION("origin and trace tags") {
    SECTION("empty trace tags") {
      const std::unordered_map<std::string, std::string> headers{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-sampling-priority", "0"},
          {"x-datadog-origin", "Egypt"},
          {"x-datadog-tags", ""}};
      MockDictReader reader{headers};
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
      MockDictWriter writer;
      span->inject(writer);

      REQUIRE(writer.items.at("x-datadog-origin") == "Egypt");
      // empty trace tags → x-datadog-tags is not set
      REQUIRE(writer.items.count("x-datadog-tags") == 0);
    }

    SECTION("lots of trace tags") {
      const std::string trace_tags =
          "foo=bar,34=43,54-46=my-number,_dd.p.not_excluded=foo";
      const std::unordered_map<std::string, std::string> headers{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-sampling-priority", "0"},
          {"x-datadog-origin", "Egypt"},
          {"x-datadog-tags", trace_tags}};
      MockDictReader reader{headers};
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
      MockDictWriter writer;
      span->inject(writer);

      REQUIRE(writer.items.at("x-datadog-origin") == "Egypt");
      REQUIRE(writer.items.count("x-datadog-tags") == 1);
      const auto output = decode_tags(writer.items.at("x-datadog-tags"));
      const auto input = decode_tags(trace_tags);
      REQUIRE(output);
      REQUIRE(input);
      // Trace tags that don't begin with "_dd.p." are excluded from the parsed
      // trace tags, so check only that the output is a subset of the input.
      REQUIRE_THAT(*input, ContainsSubset(*output));
    }
  }
}

TEST_CASE("injection can be disabled using the \"none\" style") {
  TracerConfig config;
  config.service = "testsvc";
  config.name = "spanny";
  config.collector = std::make_shared<MockCollector>();
  config.logger = std::make_shared<MockLogger>();
  config.injection_styles = {PropagationStyle::NONE};

  const auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  const auto span = tracer.create_span();
  MockDictWriter writer;
  span.inject(writer);
  const std::unordered_map<std::string, std::string> empty;
  REQUIRE(writer.items == empty);
}

TEST_CASE("injecting W3C traceparent header") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<NullCollector>();
  config.logger = std::make_shared<NullLogger>();
  config.injection_styles = {PropagationStyle::W3C};

  SECTION("extracted from W3C traceparent") {
    config.extraction_styles = {PropagationStyle::W3C};
    const auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);

    // Override the tracer's ID generator to always return `expected_parent_id`.
    constexpr std::uint64_t expected_parent_id = 0xcafebabe;
    struct Generator : public IDGenerator {
      const std::uint64_t id;
      explicit Generator(std::uint64_t id) : id(id) {}
      TraceID trace_id(const TimePoint&) const override { return TraceID(id); }
      std::uint64_t span_id() const override { return id; }
    };
    Tracer tracer{*finalized_config,
                  std::make_shared<Generator>(expected_parent_id)};

    const std::unordered_map<std::string, std::string> input_headers{
        // https://www.w3.org/TR/trace-context/#examples-of-http-traceparent-headers
        {"traceparent",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"},
    };
    const MockDictReader reader{input_headers};
    const auto maybe_span = tracer.extract_span(reader);
    REQUIRE(maybe_span);
    const auto& span = *maybe_span;
    REQUIRE(span.id() == expected_parent_id);

    MockDictWriter writer;
    span.inject(writer);
    const auto& output_headers = writer.items;
    const auto found = output_headers.find("traceparent");
    REQUIRE(found != output_headers.end());
    // The "00000000cafebabe" is the zero-padded `expected_parent_id`.
    const StringView expected =
        "00-4bf92f3577b34da6a3ce929d0e0e4736-00000000cafebabe-01";
    REQUIRE(found->second == expected);
  }

  SECTION("not extracted from W3C traceparent") {
    const auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);

    // Override the tracer's ID generator to always return a fixed value.
    constexpr std::uint64_t expected_id = 0xcafebabe;
    struct Generator : public IDGenerator {
      const std::uint64_t id;
      explicit Generator(std::uint64_t id) : id(id) {}
      TraceID trace_id(const TimePoint&) const override { return TraceID(id); }
      std::uint64_t span_id() const override { return id; }
    };
    Tracer tracer{*finalized_config, std::make_shared<Generator>(expected_id)};

    auto span = tracer.create_span();

    // Let's test the effect sampling priority plays on the resulting
    // traceparent, too.
    struct TestCase {
      int sampling_priority;
      std::string expected_flags;
    };
    const auto& [sampling_priority, expected_flags] = GENERATE(
        values<TestCase>({{-1, "00"}, {0, "00"}, {1, "01"}, {2, "01"}}));

    CAPTURE(sampling_priority);
    CAPTURE(expected_flags);

    span.trace_segment().override_sampling_priority(sampling_priority);

    MockDictWriter writer;
    span.inject(writer);
    const auto& output_headers = writer.items;
    const auto found = output_headers.find("traceparent");
    REQUIRE(found != output_headers.end());
    // The "cafebabe"s come from `expected_id`.
    const std::string expected =
        "00-000000000000000000000000cafebabe-00000000cafebabe-" +
        expected_flags;
    REQUIRE(found->second == expected);
  }
}

TEST_CASE("injecting W3C tracestate header") {
  // Concerns:
  // - the basics:
  //   - sampling priority
  //   - origin
  //   - trace tags
  //   - parent id
  //   - extra fields (extracted from W3C)
  //   - all of the above
  // - character substitutions:
  //   - in origin
  //   - in trace tag key
  //   - in trace tag value
  //     - special tilde ("~") behavior
  // - length limit:
  //   - at origin
  //   - at a trace tag
  //   - at the extra fields (extracted from W3C)

  TracerConfig config;
  config.service = "testsvc";
  // The order of the extraction styles doesn't matter for this test, because
  // it'll either be one or the other in the test cases.
  config.extraction_styles = {PropagationStyle::DATADOG, PropagationStyle::W3C};
  config.injection_styles = {PropagationStyle::W3C};
  // If one of these test cases results in a local sampling decision, let it be
  // "drop."
  config.trace_sampler.sample_rate = 0.0;
  const auto logger = std::make_shared<MockLogger>();
  config.logger = logger;
  config.collector = std::make_shared<NullCollector>();

  const auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config};

  struct TestCase {
    int line;
    std::string name;
    std::unordered_map<std::string, std::string> input_headers;
    std::string expected_tracestate;
  };

  static const auto traceparent_drop =
      "00-00000000000000000000000000000001-0000000000000001-00";

  // clang-format off
  auto test_case = GENERATE(values<TestCase>({
    {__LINE__, "sampling priority",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-sampling-priority", "2"}},
     "dd=s:2;p:$parent_id"},

    {__LINE__, "origin",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-origin", "France"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;o:France"},

    {__LINE__, "trace tags",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-tags", "_dd.p.foo=x,_dd.p.bar=y,ignored=wrong_prefix"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;t.foo:x;t.bar:y"},

    {__LINE__, "extra fields",
     {{"traceparent", traceparent_drop}, {"tracestate", "dd=foo:bar;boing:boing"}},
    // The "s:0" comes from the sampling decision in `traceparent_drop`.
    "dd=s:0;p:$parent_id;foo:bar;boing:boing"},

    {__LINE__, "all of the above",
     {{"traceparent", traceparent_drop},
      {"tracestate", "dd=o:France;t.foo:x;t.bar:y;foo:bar;boing:boing"}},
    // The "s:0" comes from the sampling decision in `traceparent_drop`.
    "dd=s:0;p:$parent_id;o:France;t.foo:x;t.bar:y;foo:bar;boing:boing"},

    {__LINE__, "replace invalid characters in origin",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-origin", "France, is a country=nation; so is 台北."}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;o:France_ is a country~nation_ so is ______."},

    {__LINE__, "replace invalid characters in trace tag key",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-tags", "_dd.p.a;d台北x =foo,_dd.p.ok=bar"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;t.a_d______x_:foo;t.ok:bar"},

    {__LINE__, "replace invalid characters in trace tag value",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-tags", "_dd.p.wacky=hello fr~d; how are คุณ?"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;t.wacky:hello fr_d_ how are _________?"},

    {__LINE__, "replace equal signs with tildes in trace tag value",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-tags", "_dd.p.base64_thingy=d2Fra2EhIHdhaw=="}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;t.base64_thingy:d2Fra2EhIHdhaw~~"},

    {__LINE__, "oversized origin truncates it and subsequent fields",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-origin", "long cat is looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"},
      {"x-datadog-tags", "_dd.p.foo=bar,_dd.p.honk=honk"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id"},

    {__LINE__, "oversized trace tag truncates it and subsequent fields",
     {{"x-datadog-trace-id", "1"}, {"x-datadog-parent-id", "1"},
      {"x-datadog-tags", "_dd.p.foo=bar,_dd.p.long_cat_is=looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong,_dd.p.lost=forever"}},
      // The "s:-1" comes from the 0% sample rate.
     "dd=s:-1;p:$parent_id;t.foo:bar"},

    {__LINE__, "oversized extra field truncates itself and subsequent fields",
     {{"traceparent", traceparent_drop},
      {"tracestate", "dd=foo:bar;long_cat_is:looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong;lost:forever"}},
     // The "s:0" comes from the sampling decision in `traceparent_drop`.
     "dd=s:0;p:$parent_id;foo:bar"},

    {__LINE__, "non-Datadog tracestate",
     {{"traceparent", traceparent_drop},
      {"tracestate", "foo=bar,boing=boing"}},
     // The "s:0" comes from the sampling decision in `traceparent_drop`.
     "dd=s:0;p:$parent_id,foo=bar,boing=boing"},
  }));
  // clang-format on

  CAPTURE(test_case.name);
  CAPTURE(test_case.line);
  CAPTURE(test_case.input_headers);
  CAPTURE(test_case.expected_tracestate);
  CAPTURE(logger->entries);

  MockDictReader reader{test_case.input_headers};
  const auto span = tracer.extract_span(reader);
  REQUIRE(span);

  MockDictWriter writer;
  span->inject(writer);

  CAPTURE(writer.items);
  const auto found = writer.items.find("tracestate");
  REQUIRE(found != writer.items.end());

  test_case.expected_tracestate.replace(
      test_case.expected_tracestate.find("$parent_id"),
      sizeof("$parent_id") - 1, hex_padded(span->id()));
  REQUIRE(found->second == test_case.expected_tracestate);

  REQUIRE(logger->error_count() == 0);
}

TEST_CASE("128-bit trace ID injection") {
  TracerConfig config;
  config.service = "testsvc";
  config.logger = std::make_shared<MockLogger>();
  config.generate_128bit_trace_ids = true;

  std::vector<PropagationStyle> injection_styles{
      PropagationStyle::W3C, PropagationStyle::DATADOG, PropagationStyle::B3};
  config.injection_styles = injection_styles;

  const auto finalized = finalize_config(config);
  REQUIRE(finalized);

  class MockIDGenerator : public IDGenerator {
    const TraceID trace_id_;

   public:
    explicit MockIDGenerator(TraceID trace_id) : trace_id_(trace_id) {}
    TraceID trace_id(const TimePoint&) const override { return trace_id_; }
    // `span_id` won't be called, because root spans use the lower part of
    // `trace_id` for the span ID.
    std::uint64_t span_id() const override { return 42; }
  };

  const TraceID trace_id{0xcafebabecafebabeULL, 0xdeadbeefdeadbeefULL};
  Tracer tracer{*finalized, std::make_shared<MockIDGenerator>(trace_id)};

  auto span = tracer.create_span();
  span.trace_segment().override_sampling_priority(2);
  MockDictWriter writer;
  span.inject(writer);

  // PropagationStyle::DATADOG
  auto found = writer.items.find("x-datadog-trace-id");
  REQUIRE(found != writer.items.end());
  REQUIRE(found->second == std::to_string(trace_id.low));
  found = writer.items.find("x-datadog-tags");
  REQUIRE(found != writer.items.end());
  REQUIRE(found->second.find("_dd.p.tid=deadbeefdeadbeef") !=
          std::string::npos);

  // PropagationStyle::W3C
  found = writer.items.find("traceparent");
  REQUIRE(found != writer.items.end());
  REQUIRE(found->second ==
          "00-deadbeefdeadbeefcafebabecafebabe-cafebabecafebabe-01");

  // PropagationStyle::B3
  found = writer.items.find("x-b3-traceid");
  REQUIRE(found != writer.items.end());
  REQUIRE(found->second == "deadbeefdeadbeefcafebabecafebabe");
}

TEST_CASE("sampling delegation injection") {
  TracerConfig config;
  config.service = "testsvc";
  config.logger = std::make_shared<MockLogger>();
  config.collector = std::make_shared<NullCollector>();

  SECTION("configuration") {
    config.delegate_trace_sampling = true;
    const auto finalized = finalize_config(config);
    REQUIRE(finalized);

    Tracer tracer{*finalized};

    SECTION("delegate_trace_sampling inject header") {
      auto span = tracer.create_span();
      MockDictWriter writer;
      span.inject(writer);

      auto found = writer.items.find("x-datadog-delegate-trace-sampling");
      REQUIRE(found != writer.items.cend());
      REQUIRE(found->second == "delegate");
    }

    SECTION("injection option override sampling delegation configuration") {
      const InjectionOptions options{/* delegate_sampling_decision=*/false};
      auto span = tracer.create_span();
      MockDictWriter writer;
      span.inject(writer, options);

      REQUIRE(0 == writer.items.count("x-datadog-delegate-trace-sampling"));
    }
  }

  SECTION("injection options") {
    const auto finalized = finalize_config(config);
    REQUIRE(finalized);

    Tracer tracer{*finalized};
    MockDictWriter writer;
    InjectionOptions options;

    options.delegate_sampling_decision = true;
    auto span = tracer.create_span();
    span.inject(writer, options);

    auto found = writer.items.find("x-datadog-delegate-trace-sampling");
    REQUIRE(found != writer.items.cend());
    REQUIRE(found->second == "delegate");
  }

  SECTION("end-to-end") {
    config.delegate_trace_sampling = true;
    const auto finalized = finalize_config(config);
    REQUIRE(finalized);

    Tracer tracer{*finalized};

    auto root_span = tracer.create_span();

    MockDictWriter writer;
    root_span.inject(writer);
    auto found = writer.items.find("x-datadog-delegate-trace-sampling");
    REQUIRE(found != writer.items.cend());
    REQUIRE(found->second == "delegate");

    MockDictReader reader(writer.items);
    auto sub_span = tracer.extract_span(reader);
    REQUIRE(!sub_span->trace_segment().sampling_decision());

    MockDictWriter response_writer;
    sub_span->trace_segment().write_sampling_delegation_response(
        response_writer);
    REQUIRE(1 ==
            response_writer.items.count("x-datadog-trace-sampling-decision"));

    MockDictReader response_reader(response_writer.items);
    SECTION("default") {
      REQUIRE(root_span.read_sampling_delegation_response(response_reader));

      // If no manual sampling override was made locally, then expect that the
      // decision read above will be the one applied.
      auto root_sampling_decision =
          root_span.trace_segment().sampling_decision();
      REQUIRE(root_sampling_decision);
      REQUIRE(root_sampling_decision->origin ==
              SamplingDecision::Origin::DELEGATED);
      REQUIRE(root_sampling_decision->priority ==
              sub_span->trace_segment().sampling_decision()->priority);
    }

    SECTION("manual sampling override") {
      root_span.trace_segment().override_sampling_priority(-1);
      REQUIRE(root_span.read_sampling_delegation_response(response_reader));

      // If `override_sampling_priority` was called on this segment, then any
      // decision read above will not replace the override.
      auto root_sampling_decision =
          root_span.trace_segment().sampling_decision();
      REQUIRE(root_sampling_decision);
      REQUIRE(root_sampling_decision->origin ==
              SamplingDecision::Origin::LOCAL);
      REQUIRE(root_sampling_decision->mechanism ==
              static_cast<int>(SamplingMechanism::MANUAL));
    }
  }
}
