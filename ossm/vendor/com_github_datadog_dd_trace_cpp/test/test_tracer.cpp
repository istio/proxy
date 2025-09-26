// These are tests for `Tracer`.  `Tracer` is responsible for creating root
// spans and for extracting spans from propagated trace context.

#include <datadog/error.h>
#include <datadog/hex.h>
#include <datadog/null_collector.h>
#include <datadog/optional.h>
#include <datadog/parse_util.h>
#include <datadog/platform_util.h>
#include <datadog/span.h>
#include <datadog/span_config.h>
#include <datadog/span_data.h>
#include <datadog/span_defaults.h>
#include <datadog/tag_propagation.h>
#include <datadog/tags.h>
#include <datadog/trace_id.h>
#include <datadog/trace_segment.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>
#include <datadog/w3c_propagation.h>

#include <chrono>
#include <ctime>
#include <iosfwd>
#include <stdexcept>
#include <utility>

#include "matchers.h"
#include "mocks/collectors.h"
#include "mocks/dict_readers.h"
#include "mocks/dict_writers.h"
#include "mocks/loggers.h"
#include "test.h"

namespace datadog {
namespace tracing {

std::ostream& operator<<(std::ostream& stream,
                         const Optional<Error::Code>& code) {
  if (code) {
    return stream << "Error::Code(" << int(*code) << ")";
  }
  return stream << "null";
}

}  // namespace tracing
}  // namespace datadog

using namespace datadog::tracing;

// Verify that the `.defaults.*` (`SpanDefaults`) properties of a tracer's
// configuration do determine the default properties of spans created by the
// tracer.
TEST_CASE("tracer span defaults") {
  TracerConfig config;
  config.service = "foosvc";
  config.service_type = "crawler";
  config.environment = "swamp";
  config.version = "first";
  config.name = "test.thing";
  config.tags = {{"some.thing", "thing value"},
                 {"another.thing", "another value"}};

  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  const auto logger = std::make_shared<MockLogger>();
  config.logger = logger;

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);

  Tracer tracer{*finalized_config};

  // Some of the sections below will override the defaults using `overrides`.
  // Make sure that the overridden values are different from the defaults,
  // so that we can distinguish between them.
  SpanConfig overrides;
  overrides.service = "barsvc";
  overrides.service_type = "wiggler";
  overrides.environment = "desert";
  overrides.version = "second";
  overrides.name = "test.another.thing";
  overrides.tags = {{"different.thing", "different"},
                    {"another.thing", "different value"}};

  REQUIRE(overrides.service != config.service);
  REQUIRE(overrides.service_type != config.service_type);
  REQUIRE(overrides.environment != config.environment);
  REQUIRE(overrides.version != config.version);
  REQUIRE(overrides.name != config.name);
  REQUIRE(overrides.tags != config.tags);

  // Some of the sections below create a span from extracted trace context.
  const std::unordered_map<std::string, std::string> headers{
      {"x-datadog-trace-id", "123"}, {"x-datadog-parent-id", "456"}};
  const MockDictReader reader{headers};

  SECTION("are honored in a root span") {
    {
      auto root = tracer.create_span();
      (void)root;
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the configured default values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    auto& root_ptr = chunk.front();
    REQUIRE(root_ptr);
    const auto& root = *root_ptr;

    REQUIRE(root.service == config.service);
    REQUIRE(root.service_type == config.service_type);
    REQUIRE(root.environment() == config.environment);
    REQUIRE(root.version() == config.version);
    REQUIRE(root.name == config.name);
    REQUIRE_THAT(root.tags, ContainsSubset(*config.tags));
  }

  SECTION("can be overridden in a root span") {
    {
      auto root = tracer.create_span(overrides);
      (void)root;
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the overridden values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    auto& root_ptr = chunk.front();
    REQUIRE(root_ptr);
    const auto& root = *root_ptr;

    REQUIRE(root.service == overrides.service);
    REQUIRE(root.service_type == overrides.service_type);
    REQUIRE(root.environment() == overrides.environment);
    REQUIRE(root.version() == overrides.version);
    REQUIRE(root.name == overrides.name);
    REQUIRE_THAT(root.tags, ContainsSubset(overrides.tags));
  }

  SECTION("are honored in an extracted span") {
    {
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the configured default values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;

    REQUIRE(span.service == config.service);
    REQUIRE(span.service_type == config.service_type);
    REQUIRE(span.environment() == config.environment);
    REQUIRE(span.version() == config.version);
    REQUIRE(span.name == config.name);
    REQUIRE_THAT(span.tags, ContainsSubset(*config.tags));
  }

  SECTION("can be overridden in an extracted span") {
    {
      auto span = tracer.extract_span(reader, overrides);
      REQUIRE(span);
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the configured default values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    REQUIRE(chunk.size() == 1);
    auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    const auto& span = *span_ptr;

    REQUIRE(span.service == overrides.service);
    REQUIRE(span.service_type == overrides.service_type);
    REQUIRE(span.environment() == overrides.environment);
    REQUIRE(span.version() == overrides.version);
    REQUIRE(span.name == overrides.name);
    REQUIRE_THAT(span.tags, ContainsSubset(overrides.tags));
  }

  SECTION("are honored in a child span") {
    {
      auto parent = tracer.create_span();
      auto child = parent.create_child();
      (void)child;
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the configured default values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    // One span for the parent, and another for the child.
    REQUIRE(chunk.size() == 2);
    // The parent will be first, so the child is last.
    auto& child_ptr = chunk.back();
    REQUIRE(child_ptr);
    const auto& child = *child_ptr;

    REQUIRE(child.service == config.service);
    REQUIRE(child.service_type == config.service_type);
    REQUIRE(child.environment() == config.environment);
    REQUIRE(child.version() == config.version);
    REQUIRE(child.name == config.name);
    REQUIRE_THAT(child.tags, ContainsSubset(*config.tags));
  }

  SECTION("can be overridden in a child span") {
    {
      auto parent = tracer.create_span();
      auto child = parent.create_child(overrides);
      (void)child;
    }
    REQUIRE(logger->error_count() == 0);

    // Get the finished span from the collector and verify that its
    // properties have the configured default values.
    REQUIRE(collector->chunks.size() == 1);
    const auto& chunk = collector->chunks.front();
    // One span for the parent, and another for the child.
    REQUIRE(chunk.size() == 2);
    // The parent will be first, so the child is last.
    auto& child_ptr = chunk.back();
    REQUIRE(child_ptr);
    const auto& child = *child_ptr;

    REQUIRE(child.service == overrides.service);
    REQUIRE(child.service_type == overrides.service_type);
    REQUIRE(child.environment() == overrides.environment);
    REQUIRE(child.version() == overrides.version);
    REQUIRE(child.name == overrides.name);
    REQUIRE_THAT(child.tags, ContainsSubset(overrides.tags));
  }
}

TEST_CASE("span extraction") {
  TracerConfig config;
  config.service = "testsvc";
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();

  SECTION(
      "extract_or_create yields a root span when there's no context to "
      "extract") {
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    const std::unordered_map<std::string, std::string> no_headers;
    MockDictReader reader{no_headers};
    auto span = tracer.extract_or_create_span(reader);
    REQUIRE(span);
    REQUIRE(!span->parent_id());
  }

  SECTION("extraction failures") {
    struct TestCase {
      int line;
      std::string name;
      std::vector<PropagationStyle> extraction_styles;
      std::unordered_map<std::string, std::string> headers;
      // Null means "don't expect an error."
      Optional<Error::Code> expected_error;
    };

    auto test_case = GENERATE(values<TestCase>({
        {__LINE__,
         "no span",
         {PropagationStyle::DATADOG},
         {},
         Error::NO_SPAN_TO_EXTRACT},
        {__LINE__,
         "missing trace ID",
         {PropagationStyle::DATADOG},
         {{"x-datadog-parent-id", "456"}},
         Error::MISSING_TRACE_ID},
        {__LINE__,
         "missing parent span ID",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "123"}},
         Error::MISSING_PARENT_SPAN_ID},
        {__LINE__,
         "missing parent span ID, but it's ok because origin",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "123"}, {"x-datadog-origin", "anything"}},
         nullopt},
        {__LINE__,
         "bad x-datadog-trace-id",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "f"}, {"x-datadog-parent-id", "456"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-datadog-trace-id (2)",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "99999999999999999999999999"},
          {"x-datadog-parent-id", "456"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "bad x-datadog-parent-id",
         {PropagationStyle::DATADOG},
         {{"x-datadog-parent-id", "f"}, {"x-datadog-trace-id", "456"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-datadog-parent-id (2)",
         {PropagationStyle::DATADOG},
         {{"x-datadog-parent-id", "99999999999999999999999999"},
          {"x-datadog-trace-id", "456"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "bad x-datadog-sampling-priority",
         {PropagationStyle::DATADOG},
         {{"x-datadog-parent-id", "123"},
          {"x-datadog-trace-id", "456"},
          {"x-datadog-sampling-priority", "keep"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-datadog-sampling-priority (2)",
         {PropagationStyle::DATADOG},
         {{"x-datadog-parent-id", "123"},
          {"x-datadog-trace-id", "456"},
          {"x-datadog-sampling-priority", "99999999999999999999999999"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "bad x-b3-traceid",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "0xdeadbeef"}, {"x-b3-spanid", "def"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-b3-traceid (2)",
         {PropagationStyle::B3},
         {{"x-b3-traceid",
           "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
          {"x-b3-spanid", "def"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "bad x-b3-spanid",
         {PropagationStyle::B3},
         {{"x-b3-spanid", "0xdeadbeef"}, {"x-b3-traceid", "def"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-b3-spanid (2)",
         {PropagationStyle::B3},
         {{"x-b3-spanid", "ffffffffffffffffffffffffffffff"},
          {"x-b3-traceid", "def"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "bad x-b3-sampled",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "abc"},
          {"x-b3-spanid", "def"},
          {"x-b3-sampled", "true"}},
         Error::INVALID_INTEGER},
        {__LINE__,
         "bad x-b3-sampled (2)",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "abc"},
          {"x-b3-spanid", "def"},
          {"x-b3-sampled", "99999999999999999999999999"}},
         Error::OUT_OF_RANGE_INTEGER},
        {__LINE__,
         "zero x-datadog-trace-id",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "0"},
          {"x-datadog-parent-id", "1234"},
          {"x-datadog-sampling-priority", "0"}},
         Error::ZERO_TRACE_ID},
        {__LINE__,
         "zero x-b3-traceid",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "0"}, {"x-b3-spanid", "123"}, {"x-b3-sampled", "0"}},
         Error::ZERO_TRACE_ID},
        {__LINE__,
         "character encoding",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "\xFD\xD0\x6C\x6C\x6F\x2C\x20\xC3\xB1\x21"},
          {"x-datadog-parent-id", "1234"},
          {"x-datadog-sampling-priority", "0"}},
         Error::INVALID_INTEGER},
    }));

    CAPTURE(test_case.line);
    CAPTURE(test_case.name);

    config.extraction_styles = test_case.extraction_styles;
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    MockDictReader reader{test_case.headers};

    auto result = tracer.extract_span(reader);
    if (test_case.expected_error) {
      REQUIRE(!result);
      REQUIRE(result.error().code == test_case.expected_error);
    } else {
      REQUIRE(result);
    }

    // `extract_or_create_span` has similar behavior.
    if (test_case.expected_error != Error::NO_SPAN_TO_EXTRACT) {
      auto method = "extract_or_create_span";
      CAPTURE(method);
      auto result = tracer.extract_span(reader);
      if (test_case.expected_error) {
        REQUIRE(!result);
        REQUIRE(result.error().code == test_case.expected_error);
      } else {
        REQUIRE(result);
      }
    }
  }

  SECTION("extracted span has the expected properties") {
    struct TestCase {
      int line;
      std::string name;
      std::vector<PropagationStyle> extraction_styles;
      std::unordered_map<std::string, std::string> headers;
      TraceID expected_trace_id;
      Optional<std::uint64_t> expected_parent_id;
      Optional<int> expected_sampling_priority;
    };

    auto test_case = GENERATE(values<TestCase>({
        {__LINE__,
         "datadog style",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "123"},
          {"x-datadog-parent-id", "456"},
          {"x-datadog-sampling-priority", "2"}},
         TraceID(123),
         456,
         2},
        {__LINE__,
         "datadog style without sampling priority",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "123"}, {"x-datadog-parent-id", "456"}},
         TraceID(123),
         456,
         nullopt},
        {__LINE__,
         "datadog style without sampling priority and without parent ID",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "123"}, {"x-datadog-origin", "whatever"}},
         TraceID(123),
         nullopt,
         nullopt},
        {__LINE__,
         "B3 style",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "abc"},
          {"x-b3-spanid", "def"},
          {"x-b3-sampled", "0"}},
         TraceID(0xabc),
         0xdef,
         0},
        {__LINE__,
         "B3 style without sampling priority",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "abc"}, {"x-b3-spanid", "def"}},
         TraceID(0xabc),
         0xdef,
         nullopt},
        {__LINE__,
         "Datadog overriding B3",
         {PropagationStyle::DATADOG, PropagationStyle::B3},
         {{"x-datadog-trace-id", "255"},
          {"x-datadog-parent-id", "14"},
          {"x-datadog-sampling-priority", "0"},
          {"x-b3-traceid", "fff"},
          {"x-b3-spanid", "ef"},
          {"x-b3-sampled", "0"}},
         TraceID(255),
         14,
         0},
        {__LINE__,
         "Datadog overriding B3, without sampling priority",
         {PropagationStyle::DATADOG, PropagationStyle::B3},
         {{"x-datadog-trace-id", "255"},
          {"x-datadog-parent-id", "14"},
          {"x-b3-traceid", "fff"},
          {"x-b3-spanid", "ef"}},
         TraceID(255),
         14,
         nullopt},
        {__LINE__,
         "B3 after Datadog found no context",
         {PropagationStyle::DATADOG, PropagationStyle::B3},
         {{"x-b3-traceid", "ff"}, {"x-b3-spanid", "e"}},
         TraceID(0xff),
         0xe,
         nullopt},
        {__LINE__,
         "Datadog after B3 found no context",
         {PropagationStyle::B3, PropagationStyle::DATADOG},
         {{"x-b3-traceid", "fff"}, {"x-b3-spanid", "ef"}},
         TraceID(0xfff),
         0xef,
         nullopt},
    }));

    CAPTURE(test_case.line);
    CAPTURE(test_case.name);

    config.extraction_styles = test_case.extraction_styles;
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};
    MockDictReader reader{test_case.headers};

    const auto checks = [](const TestCase& test_case, const Span& span) {
      REQUIRE(span.trace_id() == test_case.expected_trace_id);
      REQUIRE(span.parent_id() == test_case.expected_parent_id);
      if (test_case.expected_sampling_priority) {
        auto decision = span.trace_segment().sampling_decision();
        REQUIRE(decision);
        REQUIRE(decision->priority == test_case.expected_sampling_priority);
      } else {
        REQUIRE(!span.trace_segment().sampling_decision());
      }
    };

    {
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
      checks(test_case, *span);
    }
    {
      auto span = tracer.extract_or_create_span(reader);
      auto method = "extract_or_create_span";
      CAPTURE(method);
      REQUIRE(span);
      checks(test_case, *span);
    }
  }

  SECTION("extraction can be disabled using the \"none\" style") {
    config.extraction_styles = {PropagationStyle::NONE};

    const auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};
    const std::unordered_map<std::string, std::string> headers{
        // It doesn't matter which headers are present.
        // The "none" extraction style will not inspect them, and will return
        // the "no span to extract" error.
        {"X-Datadog-Trace-ID", "foo"},
        {"X-Datadog-Parent-ID", "bar"},
        {"X-Datadog-Sampling-Priority", "baz"},
        {"X-B3-TraceID", "foo"},
        {"X-B3-SpanID", "bar"},
        {"X-B3-Sampled", "baz"},
    };
    MockDictReader reader{headers};
    const auto result = tracer.extract_span(reader);
    REQUIRE(!result);
    REQUIRE(result.error().code == Error::NO_SPAN_TO_EXTRACT);
  }

  SECTION("W3C traceparent extraction") {
    const std::unordered_map<std::string, std::string> datadog_headers{
        {"x-datadog-trace-id", "18"},
        {"x-datadog-parent-id", "23"},
        {"x-datadog-sampling-priority", "-1"},
    };

    struct TestCase {
      int line;
      std::string name;
      Optional<std::string> traceparent;
      Optional<std::string> expected_error_tag_value = {};
      Optional<TraceID> expected_trace_id = {};
      Optional<std::uint64_t> expected_parent_id = {};
      Optional<int> expected_sampling_priority = {};
    };

    // clang-format off
    auto test_case = GENERATE(values<TestCase>({
        // https://www.w3.org/TR/trace-context/#examples-of-http-traceparent-headers
        {__LINE__, "valid: w3.org example 1",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01", // traceparent
         nullopt,
         *TraceID::parse_hex("4bf92f3577b34da6a3ce929d0e0e4736"), // expected_trace_id
         67667974448284343ULL, // expected_parent_id
         1}, // expected_sampling_priority

        {__LINE__, "valid: w3.org example 2",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00", // traceparent
         nullopt,
         *TraceID::parse_hex("4bf92f3577b34da6a3ce929d0e0e4736"), // expected_trace_id
         67667974448284343ULL, // expected_parent_id
         0}, // expected_sampling_priority

        {__LINE__, "valid: future version",
         "06-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00", // traceparent
         nullopt,
         *TraceID::parse_hex("4bf92f3577b34da6a3ce929d0e0e4736"), // expected_trace_id
         67667974448284343ULL, // expected_parent_id
         0}, // expected_sampling_priority

        {__LINE__, "valid: future version with extra fields",
         "06-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00-af-delta", // traceparent
         nullopt,
         *TraceID::parse_hex("4bf92f3577b34da6a3ce929d0e0e4736"), // expected_trace_id
         67667974448284343ULL, // expected_parent_id
         0}, // expected_sampling_priority

        {__LINE__, "valid: leading and trailing spaces",
         "    00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01 \t", // traceparent
         nullopt,
         *TraceID::parse_hex("4bf92f3577b34da6a3ce929d0e0e4736"), // expected_trace_id
         67667974448284343ULL, // expected_parent_id
         1}, // expected_sampling_priority

        {__LINE__, "no traceparent",
         nullopt}, // traceparent

        {__LINE__, "invalid: not enough fields",
         "06-4bf92f3577b34da6a3ce929d0e0e4736", // traceparent
         "malformed_traceparent"}, // expected_error_tag_value

        {__LINE__, "invalid: missing hyphen",
         "064bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00", // traceparent
         "malformed_traceparent"}, // expected_error_tag_value

        {__LINE__, "invalid: extra data not preceded by hyphen",
         "06-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00af-delta", // traceparent
         "malformed_traceparent"}, // expected_error_tag_value

        {__LINE__, "invalid: version",
         "ff-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00", // traceparent
         "invalid_version"}, // expected_error_tag_value

        {__LINE__, "invalid: trace ID zero",
         "00-00000000000000000000000000000000-00f067aa0ba902b7-00", // traceparent
         "trace_id_zero"}, // expected_error_tag_value

        {__LINE__, "invalid: parent ID zero",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-00", // traceparent
         "parent_id_zero"}, // expected_error_tag_value

        {__LINE__, "invalid: trailing characters when version is zero",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00-foo", // traceparent
         "malformed_traceparent"}, // expected_error_tag_value
    }));
    // clang-format on

    CAPTURE(test_case.name);
    CAPTURE(test_case.line);

    config.extraction_styles = {PropagationStyle::W3C,
                                PropagationStyle::DATADOG};
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    auto headers = datadog_headers;
    if (test_case.traceparent) {
      headers["traceparent"] = *test_case.traceparent;
    }
    MockDictReader reader{headers};

    // We can't `span->lookup(tags::internal::w3c_extraction_error)`, because
    // that tag is internal and will not be returned by `lookup`.  Instead, we
    // finish (destroy) the span to send it to a collector, and then inspect the
    // `SpanData` at the collector.
    Optional<SamplingDecision> decision;
    {
      auto span = tracer.extract_span(reader);
      REQUIRE(span);
      decision = span->trace_segment().sampling_decision();
    }

    REQUIRE(collector->span_count() == 1);
    const auto& span_data = collector->first_span();

    if (test_case.expected_error_tag_value) {
      const auto error_found =
          span_data.tags.find(tags::internal::w3c_extraction_error);
      REQUIRE(error_found != span_data.tags.end());
      REQUIRE(error_found->second == *test_case.expected_error_tag_value);
      // Extraction would have fallen back to the next configured style (Datadog
      // -- see `config.extraction_styles`, above), and so the span's properties
      // should match `datadog_headers`, above.
      REQUIRE(span_data.trace_id == 18);
      REQUIRE(span_data.parent_id == 23);
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::EXTRACTED);
      REQUIRE(decision->priority == -1);
    } else if (!test_case.traceparent) {
      // There was no error extracting W3C context, but there was none to
      // extract.
      // Extraction would have fallen back to the next configured style (Datadog
      // -- see `config.extraction_styles`, above), and so the span's properties
      // should match `datadog_headers`, above.
      REQUIRE(span_data.trace_id == 18);
      REQUIRE(span_data.parent_id == 23);
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::EXTRACTED);
      REQUIRE(decision->priority == -1);
    } else {
      // W3C context was successfully extracted from traceparent header.
      REQUIRE(span_data.trace_id == *test_case.expected_trace_id);
      REQUIRE(span_data.parent_id == *test_case.expected_parent_id);
      REQUIRE(decision);
      REQUIRE(decision->origin == SamplingDecision::Origin::EXTRACTED);
      REQUIRE(decision->priority == *test_case.expected_sampling_priority);
    }
  }

  SECTION("W3C tracestate extraction") {
    // Ideally this would test the _behavior_ of W3C tracestate extraction,
    // rather than its implementation.
    // However, some of the effects of W3C tracestate extraction cannot be
    // observed except by injecting trace context, and there's a separate test
    // for W3C tracestate injection (in `test_span.cpp`).
    // Here we test the tracestate portion of the `extract_w3c` function,
    // declared in `w3c_propagation.h`.
    struct TestCase {
      int line;
      std::string name;
      std::string traceparent;
      Optional<std::string> tracestate;
      Optional<int> expected_sampling_priority = {};
      Optional<std::string> expected_origin = {};
      std::vector<std::pair<std::string, std::string>> expected_trace_tags = {};
      Optional<std::string> expected_additional_w3c_tracestate = {};
      Optional<std::string> expected_additional_datadog_w3c_tracestate = {};
      Optional<std::string> expected_datadog_w3c_parent_id = {};
    };

    static const std::string traceparent_prefix =
        "00-00000000000000000000000000000001-0000000000000001-0";
    static const std::string traceparent_drop = traceparent_prefix + "0";
    static const std::string traceparent_keep = traceparent_prefix + "1";

    auto test_case = GENERATE(values<TestCase>({
        {
            __LINE__,
            "no tracestate",
            traceparent_drop,    // traceparent
            nullopt,             // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags,
            nullopt,             // expected_additional_w3c_tracestate,
            nullopt,             // expected_additional_datadog_w3c_tracestate,
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "empty tracestate",
            traceparent_drop,    // traceparent
            "",                  // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags,
            nullopt,             // expected_additional_w3c_tracestate,
            nullopt,             // expected_additional_datadog_w3c_tracestate,
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "no dd entry",
            traceparent_drop,                       // traceparent
            "foo=hello,@thingy/thing=wah;wah;wah",  // tracestate
            0,        // expected_sampling_priority
            nullopt,  // expected_origin
            {},       // expected_trace_tags
            "foo=hello,@thingy/thing=wah;wah;wah",  // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "empty entry",
            traceparent_drop,        // traceparent
            "foo=hello,,bar=thing",  // tracestate
            0,                       // expected_sampling_priority
            nullopt,                 // expected_origin
            {},                      // expected_trace_tags
            "foo=hello,,bar=thing",  // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "malformed entry",
            traceparent_drop,               // traceparent
            "foo=hello,chicken,bar=thing",  // tracestate
            0,                              // expected_sampling_priority
            nullopt,                        // expected_origin
            {},                             // expected_trace_tags
            "foo=hello,chicken,bar=thing",  // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "stuff before dd entry",
            traceparent_drop,         // traceparent
            "foo=hello,bar=baz,dd=",  // tracestate
            0,                        // expected_sampling_priority
            nullopt,                  // expected_origin
            {},                       // expected_trace_tags
            "foo=hello,bar=baz",      // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "stuff after dd entry",
            traceparent_drop,         // traceparent
            "dd=,foo=hello,bar=baz",  // tracestate
            0,                        // expected_sampling_priority
            nullopt,                  // expected_origin
            {},                       // expected_trace_tags
            "foo=hello,bar=baz",      // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "stuff before and after dd entry",
            traceparent_drop,                                 // traceparent
            "chicken=yes,nuggets=yes,dd=,foo=hello,bar=baz",  // tracestate
            0,        // expected_sampling_priority
            nullopt,  // expected_origin
            {},       // expected_trace_tags
            "chicken=yes,nuggets=yes,foo=hello,bar=baz",  // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "dd entry with empty subentries",
            traceparent_drop,             // traceparent
            "dd=foo:bar;;;;;baz:bam;;;",  // tracestate
            0,                            // expected_sampling_priority
            nullopt,                      // expected_origin
            {},                           // expected_trace_tags
            nullopt,                      // expected_additional_w3c_tracestate
            "foo:bar;baz:bam",   // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "dd entry with malformed subentries",
            traceparent_drop,                              // traceparent
            "dd=foo:bar;chicken;chicken;baz:bam;chicken",  // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            "foo:bar;baz:bam",   // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "origin, trace tags, parent, and extra fields",
            traceparent_drop,  // traceparent
            "dd=o:France;p:00000000000d69ac;t.foo:thing1;t.bar:thing2;x:wow;y:"
            "wow",     // tracestate
            0,         // expected_sampling_priority
            "France",  // expected_origin
            {{"_dd.p.foo", "thing1"},
             {"_dd.p.bar", "thing2"}},  // expected_trace_tags
            nullopt,                    // expected_additional_w3c_tracestate
            "x:wow;y:wow",       // expected_additional_datadog_w3c_tracestate
            "00000000000d69ac",  // expected_datadog_w3c_parent_id
        },

        {
            __LINE__,
            "dd parent id is propagated even if not valid",
            traceparent_drop,         // traceparent
            "dd=p:yu7C0o3AOmbOcfXw",  // tracestate
            0,                        // expected_sampling_priority
            nullopt,                  // expected_origin
            {},                       // expected_trace_tags
            nullopt,                  // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "yu7C0o3AOmbOcfXw",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "origin with escaped equal sign",
            traceparent_drop,       // traceparent
            "dd=o:France~country",  // tracestate
            0,                      // expected_sampling_priority
            "France=country",       // expected_origin
            {},                     // expected_trace_tags
            nullopt,                // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling agree (1/4)",
            traceparent_drop,    // traceparent
            "dd=s:0",            // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling agree (2/4)",
            traceparent_drop,    // traceparent
            "dd=s:-1",           // tracestate
            -1,                  // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling agree (3/4)",
            traceparent_keep,    // traceparent
            "dd=s:1",            // tracestate
            1,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling agree (4/4)",
            traceparent_keep,    // traceparent
            "dd=s:2",            // tracestate
            2,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling disagree (1/4)",
            traceparent_drop,    // traceparent
            "dd=s:1",            // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling disagree (2/4)",
            traceparent_drop,    // traceparent
            "dd=s:2",            // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling disagree (3/4)",
            traceparent_keep,    // traceparent
            "dd=s:0",            // tracestate
            1,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "traceparent and tracestate sampling disagree (4/4)",
            traceparent_keep,    // traceparent
            "dd=s:-1",           // tracestate
            1,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "invalid sampling priority (1/2)",
            traceparent_drop,    // traceparent
            "dd=s:oops",         // tracestate
            0,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "invalid sampling priority (2/2)",
            traceparent_keep,    // traceparent
            "dd=s:oops",         // tracestate
            1,                   // expected_sampling_priority
            nullopt,             // expected_origin
            {},                  // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
            nullopt,             // expected_additional_datadog_w3c_tracestate
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },
    }));

    CAPTURE(test_case.name);
    CAPTURE(test_case.line);
    CAPTURE(test_case.traceparent);
    CAPTURE(test_case.tracestate);

    std::unordered_map<std::string, std::string> span_tags;
    MockLogger logger;
    CAPTURE(logger.entries);
    CAPTURE(span_tags);

    std::unordered_map<std::string, std::string> headers;
    headers["traceparent"] = test_case.traceparent;
    if (test_case.tracestate) {
      headers["tracestate"] = *test_case.tracestate;
    }
    MockDictReader reader{headers};

    const auto extracted = extract_w3c(reader, span_tags, logger);
    REQUIRE(extracted);

    REQUIRE(extracted->origin == test_case.expected_origin);
    REQUIRE(extracted->trace_tags == test_case.expected_trace_tags);
    REQUIRE(extracted->sampling_priority ==
            test_case.expected_sampling_priority);
    REQUIRE(extracted->additional_w3c_tracestate ==
            test_case.expected_additional_w3c_tracestate);
    REQUIRE(extracted->additional_datadog_w3c_tracestate ==
            test_case.expected_additional_datadog_w3c_tracestate);
    REQUIRE(extracted->datadog_w3c_parent_id ==
            test_case.expected_datadog_w3c_parent_id);

    REQUIRE(logger.entries.empty());
    REQUIRE(span_tags.empty());
  }

  SECTION("_dd.parent_id") {
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    std::unordered_map<std::string, std::string> headers;
    headers["traceparent"] =
        "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01";
    headers["tracestate"] = "dd=s:1;p:000000000000002a;foo:bar,lol=wut";
    MockDictReader reader{headers};
    const auto span = tracer.extract_span(reader);

    auto parent_id_tag = span->lookup_tag("_dd.parent_id");
    REQUIRE(parent_id_tag);
    CHECK(*parent_id_tag == "000000000000002a");
  }

  SECTION("x-datadog-tags") {
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    std::unordered_map<std::string, std::string> headers{
        {"x-datadog-trace-id", "123"}, {"x-datadog-parent-id", "456"}};
    MockDictReader reader{headers};

    SECTION("extraction succeeds when x-datadog-tags is valid") {
      const std::string header_value = "foo=bar,_dd.something=yep-yep";
      REQUIRE(decode_tags(header_value));
      headers["x-datadog-tags"] = header_value;
      REQUIRE(tracer.extract_span(reader));
    }

    SECTION("extraction succeeds when x-datadog-tags is empty") {
      const std::string header_value = "";
      REQUIRE(decode_tags(header_value));
      headers["x-datadog-tags"] = header_value;
      REQUIRE(tracer.extract_span(reader));
    }

    SECTION("extraction succeeds when x-datadog-tags is invalid") {
      const std::string header_value = "this is missing an equal sign";
      REQUIRE(!decode_tags(header_value));
      headers["x-datadog-tags"] = header_value;
      REQUIRE(tracer.extract_span(reader));
    }

    SECTION("invalid _dd.p.tid") {
      const std::string header_value =
          "_dd.p.foobar=hello,_dd.p.tid=invalidhex";
      REQUIRE(decode_tags(header_value));
      headers["x-datadog-tags"] = header_value;

      SECTION("is not propagated") {
        auto maybe_span = tracer.extract_span(reader);
        REQUIRE(maybe_span);
        auto& span = *maybe_span;

        MockDictWriter writer;
        span.inject(writer);
        // Expect a valid "x-datadog-tags" header, and it will contain
        // "_dd.p.foobar", but not "_dd.p.tid".
        REQUIRE(writer.items.count("x-datadog-tags") == 1);
        const std::string& injected_header_value =
            writer.items.find("x-datadog-tags")->second;
        const auto decoded_tags = decode_tags(injected_header_value);
        REQUIRE(decoded_tags);
        CAPTURE(*decoded_tags);
        const std::unordered_multimap<std::string, std::string> tags{
            decoded_tags->begin(), decoded_tags->end()};
        REQUIRE(tags.count("_dd.p.foobar") == 1);
        REQUIRE(tags.find("_dd.p.foobar")->second == "hello");
        REQUIRE(tags.count("_dd.p.tid") == 0);
      }

      SECTION("is noted in an error tag value") {
        {
          auto maybe_span = tracer.extract_span(reader);
          REQUIRE(maybe_span);
        }
        // Now that the span is destroyed, it will have been sent to the
        // collector.
        // We can inspect the `SpanData` in the collector to verify that the
        // `tags::internal::propagation_error` ("_dd.propagation_error") tag
        // is set to the expected value.
        const SpanData& span = collector->first_span();
        REQUIRE(span.tags.count(tags::internal::propagation_error) == 1);
        REQUIRE(span.tags.find(tags::internal::propagation_error)->second ==
                "malformed_tid invalidhex");
      }
    }
  }

  SECTION("inject an extracted span that delegated sampling") {
    config.delegate_trace_sampling = GENERATE(true, false);
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    std::unordered_map<std::string, std::string> headers{
        {"x-datadog-trace-id", "123"},
        {"x-datadog-parent-id", "456"},
        {"x-datadog-sampling-priority", "2"},
        {"x-datadog-delegate-trace-sampling", "delegate"}};

    MockDictReader reader{headers};
    auto span = tracer.extract_span(reader);
    REQUIRE(span);

    if (*config.delegate_trace_sampling) {
      REQUIRE(!span->trace_segment().sampling_decision());
    } else {
      REQUIRE(span->trace_segment().sampling_decision());
    }

    MockDictWriter writer;
    span->inject(writer);

    CAPTURE(writer.items);
    if (*config.delegate_trace_sampling) {
      // If sampling delegation is enabled, then expect the delegation header to
      // have been injected.
      auto found = writer.items.find("x-datadog-delegate-trace-sampling");
      REQUIRE(found != writer.items.end());
      REQUIRE(found->second == "delegate");
    } else {
      // Even though `span` was extracted from context that requested sampling
      // delegation, delegation is not enabled for this tracer, so expect that
      // the delegation header was not injected.
      REQUIRE(writer.items.count("x-datadog-delegate-trace-sampling") == 0);
    }
  }
}

TEST_CASE("report hostname") {
  TracerConfig config;
  config.service = "testsvc";
  config.collector = std::make_shared<NullCollector>();
  config.logger = std::make_shared<NullLogger>();

  SECTION("is off by default") {
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};
    REQUIRE(!tracer.create_span().trace_segment().hostname());
  }

  SECTION("is available when enabled") {
    config.report_hostname = true;
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};
    REQUIRE(tracer.create_span().trace_segment().hostname() == get_hostname());
  }
}

TEST_CASE("128-bit trace IDs") {
  // Use a clock that always returns a hard-coded `TimePoint`.
  // May 6, 2010 14:45:13 America/New_York
  const std::time_t flash_crash = 1273171513;
  const Clock clock = [flash_crash = flash_crash] {
    TimePoint result;
    result.wall = std::chrono::system_clock::from_time_t(flash_crash);
    return result;
  };

  TracerConfig config;
  config.service = "testsvc";
  config.generate_128bit_trace_ids = true;
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  const auto logger = std::make_shared<MockLogger>();
  config.logger = logger;
  std::vector<PropagationStyle> extraction_styles{
      PropagationStyle::W3C, PropagationStyle::DATADOG, PropagationStyle::B3};
  config.extraction_styles = extraction_styles;
  const auto finalized = finalize_config(config, clock);
  REQUIRE(finalized);
  Tracer tracer{*finalized};
  TraceID trace_id;  // used below the following SECTIONs

  SECTION("are generated") {
    // Specifically, verify that the high 64 bits of the generated trace ID
    // contain the unix start time of the trace shifted up 32 bits.
    //
    // Due to the definition of `clock`, above, that unix time will be
    // `flash_crash`.
    const auto span = tracer.create_span();
    const std::uint64_t expected = std::uint64_t(flash_crash) << 32;
    REQUIRE(span.trace_id().high == expected);
    trace_id = span.trace_id();
  }

  SECTION("are extracted from W3C") {
    std::unordered_map<std::string, std::string> headers;
    headers["traceparent"] =
        "00-deadbeefdeadbeefcafebabecafebabe-0000000000000001-01";
    MockDictReader reader{headers};
    const auto span = tracer.extract_span(reader);
    CAPTURE(logger->entries);
    REQUIRE(logger->error_count() == 0);
    REQUIRE(span);
    REQUIRE(hex(span->trace_id().high) == "deadbeefdeadbeef");
    trace_id = span->trace_id();
  }

  SECTION("are, for W3C, extracted preferentially from traceparent") {
    auto tid = GENERATE(values<std::string>({"decade", "deadbeefdeadbeed"}));
    std::unordered_map<std::string, std::string> headers;
    headers["traceparent"] =
        "00-deadbeefdeadbeefcafebabecafebabe-0000000000000001-01";
    // The _dd.p.tid value below is either malformed or inconsistent with the
    // trace ID in the traceparent.
    // It will be ignored, and the resulting _dd.p.tid value will be consistent
    // with the higher part of the trace ID in traceparent: "deadbeefdeadbeef".
    headers["tracestate"] = "dd=t.tid:" + tid + ";p:0000000000000001";
    MockDictReader reader{headers};
    const auto span = tracer.extract_span(reader);
    CAPTURE(logger->entries);
    REQUIRE(logger->error_count() == 0);
    REQUIRE(span);
    REQUIRE(hex(span->trace_id().high) == "deadbeefdeadbeef");
    trace_id = span->trace_id();
  }

  SECTION("are extracted from Datadog (_dd.p.tid)") {
    std::unordered_map<std::string, std::string> headers;
    headers["x-datadog-trace-id"] = "4";
    headers["x-datadog-parent-id"] = "42";
    headers["x-datadog-tags"] = "_dd.p.tid=000000000000beef";
    MockDictReader reader{headers};
    const auto span = tracer.extract_span(reader);
    CAPTURE(logger->entries);
    REQUIRE(logger->error_count() == 0);
    REQUIRE(span);
    REQUIRE(span->trace_id().hex_padded() ==
            "000000000000beef0000000000000004");
    trace_id = span->trace_id();
  }

  SECTION("are extracted from B3") {
    std::unordered_map<std::string, std::string> headers;
    headers["x-b3-traceid"] = "deadbeefdeadbeefcafebabecafebabe";
    headers["x-b3-spanid"] = "42";
    MockDictReader reader{headers};
    const auto span = tracer.extract_span(reader);
    CAPTURE(logger->entries);
    REQUIRE(logger->error_count() == 0);
    REQUIRE(span);
    REQUIRE(hex(span->trace_id().high) == "deadbeefdeadbeef");
    trace_id = span->trace_id();
  }

  // For any 128-bit trace ID, the _dd.p.tid trace tag is always sent to the
  // collector.
  CAPTURE(logger->entries);
  REQUIRE(logger->error_count() == 0);
  REQUIRE(collector->span_count() == 1);
  const auto& span = collector->first_span();
  const auto found = span.tags.find(tags::internal::trace_id_high);
  REQUIRE(found != span.tags.end());
  const auto high = parse_uint64(found->second, 16);
  REQUIRE(high);
  REQUIRE(*high == trace_id.high);
}

TEST_CASE(
    "_dd.p.tid invalid or inconsistent with trace ID results in error tag") {
  struct TestCase {
    int line;
    std::string name;
    std::string tid_tag_value;
    std::string expected_error_prefix;
  };

  auto test_case = GENERATE(values<TestCase>(
      {{__LINE__, "invalid _dd.p.tid", "noodle", "malformed_tid "},
       {__LINE__, "short _dd.p.tid", "beef", "malformed_tid "},
       {__LINE__, "long _dd.p.tid", "000000000000000000beef", "malformed_tid "},
       {__LINE__, "_dd.p.tid inconsistent with trace ID", "0000000000adfeed",
        "inconsistent_tid "}}));

  CAPTURE(test_case.line);
  CAPTURE(test_case.name);

  TracerConfig config;
  config.service = "testsvc";
  config.generate_128bit_trace_ids = true;
  const auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  const auto logger = std::make_shared<MockLogger>();
  config.logger = logger;
  std::vector<PropagationStyle> extraction_styles{PropagationStyle::W3C};
  config.extraction_styles = extraction_styles;
  const auto finalized = finalize_config(config);
  REQUIRE(finalized);
  Tracer tracer{*finalized};

  std::unordered_map<std::string, std::string> headers;
  headers["traceparent"] =
      "00-deadbeefdeadbeefcafebabecafebabe-0000000000000001-01";
  headers["tracestate"] = "dd=t.tid:" + test_case.tid_tag_value;
  MockDictReader reader{headers};
  CAPTURE(logger->entries);
  {
    const auto span = tracer.extract_span(reader);
    REQUIRE(span);
  }

  REQUIRE(logger->error_count() == 0);
  REQUIRE(collector->span_count() == 1);
  const auto& span = collector->first_span();
  const auto found = span.tags.find(tags::internal::propagation_error);
  REQUIRE(found != span.tags.end());
  REQUIRE(found->second ==
          test_case.expected_error_prefix + test_case.tid_tag_value);
}

TEST_CASE("sampling delegation extraction") {
  const bool enable_sampling_delegation = GENERATE(true, false);

  CAPTURE(enable_sampling_delegation);

  const auto logger = std::make_shared<NullLogger>();
  const auto collector = std::make_shared<NullCollector>();

  TracerConfig config;
  config.service = "test-sampling-delegation";
  config.logger = logger;
  config.collector = collector;
  config.extraction_styles = {PropagationStyle::DATADOG};
  config.trace_sampler.sample_rate = 1.;
  config.delegate_trace_sampling = enable_sampling_delegation;

  auto validated_config = finalize_config(config);
  REQUIRE(validated_config);

  Tracer tracer(*validated_config);

  const std::unordered_map<std::string, std::string> headers{
      {"x-datadog-trace-id", "17491188783264004180"},
      {"x-datadog-parent-id", "3390700340160032468"},
      {"x-datadog-sampling-priority", "-1"},
      {"x-datadog-tags", "_dd.p.tid=66718e8c00000000"},
      {"x-datadog-delegate-trace-sampling", "delegate"},
  };

  MockDictReader propagation_reader{headers};
  const auto maybe_span = tracer.extract_span(propagation_reader);
  REQUIRE(maybe_span);

  auto sampling_decision = maybe_span->trace_segment().sampling_decision();
  if (enable_sampling_delegation) {
    CHECK(!sampling_decision.has_value());
  } else {
    REQUIRE(sampling_decision.has_value());
    CHECK(sampling_decision->origin == SamplingDecision::Origin::EXTRACTED);
    CHECK(sampling_decision->priority == int(SamplingPriority::USER_DROP));
  }
}

TEST_CASE("_dd.is_sampling_decider") {
  // This test involves three tracers: "service1", "service2", and "service3".
  // Each calls the next, and each produces two spans: "local_root" and "child".
  //
  //     [service1] -> [service2] -> [service3]
  //     delegate       delegate       either
  //
  // Sampling delegation is enabled for service1 and for service2.
  // Regardless of whether sampling delegation is enabled for service3, the
  // following are expected:
  //
  // - service1's local root span will contain the tag
  //   "_dd.is_sampling_decider:0", because while it is the root span, it did
  //   not make the sampling decision.
  // - service2's local root span will not contain the "dd_.is_sampling_decider"
  //   tag, because it did not make the sampling decision and was not the root
  //   span.
  // - service3's local root span will contain the tag "_dd.sampling_decider:1",
  //   because regardless of whether sampling delegation was enabled for it, it
  //   made the sampling decision, and it is not the root span.
  // - any span that is not a local root span will not contain the tag
  //   "_dd.is_sampling_decider", because that tag is only ever set on the local
  //   root span if it is set at all.
  //
  // Further, if we configure service3 to keep all of its traces, then the
  // sampling decision conveyed by all of service1, service2, and service3 will
  // be "keep" due to "rule".
  bool service3_delegation_enabled = GENERATE(true, false);

  const auto collector = std::make_shared<MockCollector>();
  const auto logger = std::make_shared<MockLogger>();

  TracerConfig config1;
  config1.collector = collector;
  config1.logger = logger;
  config1.service = "service1";
  config1.delegate_trace_sampling = true;

  TracerConfig config2;
  config2.collector = collector;
  config2.logger = logger;
  config2.service = "service2";
  config2.trace_sampler.sample_rate = 1;  // keep all traces
  config2.delegate_trace_sampling = true;

  TracerConfig config3;
  config3.collector = collector;
  config3.logger = logger;
  config3.service = "service3";
  config3.delegate_trace_sampling = service3_delegation_enabled;
  config3.trace_sampler.sample_rate = 1;  // keep all traces
  CAPTURE(config3.delegate_trace_sampling);

  auto valid_config = finalize_config(config1);
  REQUIRE(valid_config);
  Tracer tracer1{*valid_config};

  valid_config = finalize_config(config2);
  REQUIRE(valid_config);
  Tracer tracer2{*valid_config};

  valid_config = finalize_config(config3);
  REQUIRE(valid_config);
  Tracer tracer3{*valid_config};

  // The spans will communicate forwards using the propagation writer and
  // reader (trace context propagation).
  MockDictWriter propagation_writer;
  MockDictReader propagation_reader{propagation_writer.items};

  // The spans will communicate backwards using the delegation writer and reader
  // (delegation responses).
  MockDictWriter delegation_writer;
  MockDictReader delegation_reader{delegation_writer.items};

  // The following nested blocks provide scopes for each of the services.
  // service1.local_root:
  {
    SpanConfig span_config;
    span_config.name = "local_root";
    Span global_root = tracer1.create_span(span_config);

    {  // service1.child
      span_config.name = "child";
      Span service1_child = global_root.create_child(span_config);

      service1_child.inject(propagation_writer);

      {  // service2.local_root:
        span_config.name = "local_root";
        Expected<Span> service2_local_root =
            tracer2.extract_span(propagation_reader, span_config);
        REQUIRE(service2_local_root);
        {  // service2.child:
          span_config.name = "child";
          Span service2_child = service2_local_root->create_child(span_config);

          propagation_writer.items.clear();
          service2_child.inject(propagation_writer);

          {  // service3.local_root:
            span_config.name = "local_root";
            Expected<Span> service3_local_root =
                tracer3.extract_span(propagation_reader, span_config);
            REQUIRE(service3_local_root);

            {  // service3.child:
              span_config.name = "child";
              Span service3_child =
                  service3_local_root->create_child(span_config);
            }
            service3_local_root->trace_segment()
                .write_sampling_delegation_response(delegation_writer);
          }

          service2_child.read_sampling_delegation_response(delegation_reader);
        }
        delegation_writer.items.clear();
        service2_local_root->trace_segment().write_sampling_delegation_response(
            delegation_writer);
      }
      service1_child.read_sampling_delegation_response(delegation_reader);
    }
    delegation_writer.items.clear();
    global_root.trace_segment().write_sampling_delegation_response(
        delegation_writer);
  }

  // service1 (the root service) was the most recent thing to
  // `write_sampling_delegation_response`, and service1 has no delegation
  // response to deliver, so expect that there are no corresponding response
  // headers.
  {
    CAPTURE(delegation_writer.items);
    REQUIRE(delegation_writer.items.empty());
  }

  // three segments, each having two spans
  REQUIRE(collector->span_count() == 3 * 2);

  const double expected_sampling_priority = double(SamplingPriority::USER_KEEP);
  // "dm" as in the "_dd.p.dm" tag
  const std::string expected_dm =
      "-" + std::to_string(int(SamplingMechanism::RULE));

  // Check everything described in the comment at the top of this `TEST_CASE`.
  for (const auto& chunk : collector->chunks) {
    for (const auto& span_ptr : chunk) {
      REQUIRE(span_ptr);
      const SpanData& span = *span_ptr;
      CAPTURE(span.service);
      CAPTURE(span.name);
      CAPTURE(span.tags);
      CAPTURE(span.numeric_tags);

      if (span.service == "service1" && span.name == "local_root") {
        REQUIRE(span.tags.count(tags::internal::sampling_decider) == 1);
        REQUIRE(span.tags.at(tags::internal::sampling_decider) == "0");
        REQUIRE(span.numeric_tags.count(tags::internal::sampling_priority) ==
                1);
        REQUIRE(span.numeric_tags.at(tags::internal::sampling_priority) ==
                expected_sampling_priority);
        REQUIRE(span.tags.count(tags::internal::decision_maker) == 1);
        REQUIRE(span.tags.at(tags::internal::decision_maker) == expected_dm);
        continue;
      }
      if (span.service == "service1" && span.name == "child") {
        REQUIRE(span.tags.count(tags::internal::sampling_decider) == 0);
        continue;
      }
      REQUIRE(span.service != "service1");
      if (span.service == "service2" && span.name == "local_root") {
        const bool made_the_decision = service3_delegation_enabled ? 0 : 1;
        REQUIRE(span.tags.count(tags::internal::sampling_decider) ==
                made_the_decision);
        REQUIRE(span.numeric_tags.count(tags::internal::sampling_priority) ==
                1);
        REQUIRE(span.numeric_tags.at(tags::internal::sampling_priority) ==
                expected_sampling_priority);
        REQUIRE(span.tags.count(tags::internal::decision_maker) == 1);
        REQUIRE(span.tags.at(tags::internal::decision_maker) == expected_dm);
        continue;
      }
      if (span.service == "service2" && span.name == "child") {
        REQUIRE(span.tags.count(tags::internal::sampling_decider) == 0);
        continue;
      }
      REQUIRE(span.service != "service2");
      if (span.service == "service3" && span.name == "local_root") {
        const bool made_the_decision = service3_delegation_enabled ? 1 : 0;
        REQUIRE(span.tags.count(tags::internal::sampling_decider) ==
                made_the_decision);
        REQUIRE(span.numeric_tags.count(tags::internal::sampling_priority) ==
                1);
        REQUIRE(span.numeric_tags.at(tags::internal::sampling_priority) ==
                expected_sampling_priority);
        REQUIRE(span.tags.count(tags::internal::decision_maker) == 1);
        REQUIRE(span.tags.at(tags::internal::decision_maker) == expected_dm);
        continue;
      }
      if (span.service == "service3" && span.name == "child") {
        REQUIRE(span.tags.count(tags::internal::sampling_decider) == 0);
        continue;
      }
      REQUIRE(span.service != "service3");
    }
  }
}

TEST_CASE("sampling delegation is not an override") {
  // Verify that sampling delegation does not occur, even if so configured,
  // when a sampling decision is extracted from an incoming request _and_
  // sampling delegation was not indicated in that request.
  // We want to make sure that a mid-trace tracer configured to delegate
  // sampling does not "break the trace," i.e. change the sampling decision
  // mid-trace.
  //
  // This test involves three tracers: "service1", "service2", and "service3".
  // Each calls the next, and each produces one span: "local_root".
  //
  //     [service1]         -> [service2]  ->   [service3]
  //     keep/drop/neither                       keep/drop
  //     delegate?              delegate
  //

  // There are three variables:
  //
  // 1. the injected sampling decision from service1, if any,
  // 2. the configured sampling decision for service3,
  // 3. whether service1 is configured to delegate.
  //
  // When service1 is configured to delegate, the sampling decision of all
  // three services should be consistent with that made by service3.
  //
  // When service1 is configured _not_ to delegate, and when it injects a
  // sampling decision, then the sampling decision of all three services should
  // be consistent with that made by service1.
  //
  // When service1 is configured _not_ to delegate, and when it does _not_
  // inject a sampling decision, then the sampling decision of all three
  // services should be consistent with that made by service3.
  //
  // The idea is that service2 does not perform delegation when service1 already
  // made a decision and did not request delegation.
  auto service1_delegate = GENERATE(true, false);
  auto service3_sample_rate = GENERATE(0.0, 1.0);

  const int service3_sampling_priority = service3_sample_rate == 0.0 ? -1 : 2;

  CAPTURE(service1_delegate);
  CAPTURE(service3_sample_rate);

  const auto collector = std::make_shared<MockCollector>();
  const auto logger = std::make_shared<MockLogger>();
  const std::vector<PropagationStyle> styles = {PropagationStyle::DATADOG};

  TracerConfig config1;
  config1.collector = collector;
  config1.logger = logger;
  config1.extraction_styles = config1.injection_styles = styles;
  config1.service = "service1";
  config1.delegate_trace_sampling = service1_delegate;
  config1.trace_sampler.sample_rate = 1.0;  // as a default
  // `service1_sampling_priority` will be dealt with when service1 injects trace
  // context.

  TracerConfig config2;
  config2.collector = collector;
  config2.logger = logger;
  config2.extraction_styles = config1.injection_styles = styles;
  config2.service = "service2";
  config2.delegate_trace_sampling = true;

  TracerConfig config3;
  config3.collector = collector;
  config3.logger = logger;
  config3.extraction_styles = config1.injection_styles = styles;
  config3.service = "service3";
  config3.delegate_trace_sampling = true;
  config3.trace_sampler.sample_rate = service3_sample_rate;

  auto valid_config = finalize_config(config1);
  REQUIRE(valid_config);
  Tracer tracer1{*valid_config};

  valid_config = finalize_config(config2);
  REQUIRE(valid_config);
  Tracer tracer2{*valid_config};

  valid_config = finalize_config(config3);
  Tracer tracer3{*valid_config};

  // The spans will communicate forwards using the propagation writer and
  // reader (trace context propagation).
  MockDictWriter propagation_writer;
  MockDictReader propagation_reader{propagation_writer.items};

  // The spans will communicate backwards using the delegation writer and reader
  // (delegation responses).
  MockDictWriter delegation_writer;
  MockDictReader delegation_reader{delegation_writer.items};

  {
    SpanConfig span_config;
    span_config.name = "local_root";
    Span span1 = tracer1.create_span(span_config);
    span1.inject(propagation_writer);

    {
      auto span2 = tracer2.extract_span(propagation_reader, span_config);
      REQUIRE(span2);
      propagation_writer.items.clear();
      span2->inject(propagation_writer);
      const bool expected_delegate_header = service1_delegate ? 1 : 0;
      CHECK(
          propagation_writer.items.count("x-datadog-delegate-trace-sampling") ==
          expected_delegate_header);

      {
        auto span3 = tracer3.extract_span(propagation_reader, span_config);
        REQUIRE(span3);
        span3->trace_segment().write_sampling_delegation_response(
            delegation_writer);
      }

      span2->trace_segment().read_sampling_delegation_response(
          delegation_reader);
      delegation_writer.items.clear();
      span2->trace_segment().write_sampling_delegation_response(
          delegation_writer);
    }

    span1.trace_segment().read_sampling_delegation_response(delegation_reader);
  }

  // Verify that we received three spans, and that they have the expected
  // sampling priorities.
  REQUIRE(collector->span_count() == 3);
  for (const auto& chunk : collector->chunks) {
    for (const auto& span_ptr : chunk) {
      REQUIRE(span_ptr);
      const SpanData& span = *span_ptr;
      CAPTURE(span.service);
      REQUIRE(span.numeric_tags.count(tags::internal::sampling_priority) == 1);
      // If `service1_delegate` is false, then service1's sampling decision was
      // made by the service1's sampler, which will result in priority 2.
      // Otherwise, it's the same priority expected for the other spans.
      if (!service1_delegate) {
        REQUIRE(span.numeric_tags.at(tags::internal::sampling_priority) == 2);
      } else {
        REQUIRE(span.numeric_tags.at(tags::internal::sampling_priority) ==
                service3_sampling_priority);
      }
    }
  }
}

TEST_CASE("heterogeneous extraction") {
  // These test cases verify that when W3C is among the configured extraction
  // styles, then non-Datadog and unexpected Datadog fields in an incoming
  // `tracestate` are extracted, under certain conditions, even when trace
  // context was extracted in a non-W3C style.
  //
  // The idea is that a tracer might be configured to extract, e.g.,
  // [DATADOG, B3, W3C] and to inject [DATADOG, W3C]. We want to make
  // sure that no W3C-relevant information from the incoming request is lost in
  // the outgoing W3C headers, even if trace context is extracted on account of
  // DATADOG or B3.
  //
  // See the `TestCase` instances, below, for more information.

  class MockIDGenerator : public IDGenerator {
   public:
    TraceID trace_id(const TimePoint&) const override {
      throw std::logic_error("This test should not generate a trace ID.");
    }
    std::uint64_t span_id() const override { return 0x2a; }
  };

  struct TestCase {
    int line;
    std::string description;
    std::vector<PropagationStyle> extraction_styles;
    std::vector<PropagationStyle> injection_styles;
    std::unordered_map<std::string, std::string> extracted_headers;
    std::unordered_map<std::string, std::string> expected_injected_headers;
  };

  // clang-format off
  auto test_case = GENERATE(values<TestCase>({
    {__LINE__, "tracestate from primary style",
     {PropagationStyle::W3C, PropagationStyle::DATADOG},
     {PropagationStyle::W3C},
     {{"traceparent", "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"},
      {"tracestate", "dd=foo:bar,lol=wut"}},
     {{"traceparent", "00-4bf92f3577b34da6a3ce929d0e0e4736-000000000000002a-01"},
      {"tracestate", "dd=s:1;p:000000000000002a;foo:bar,lol=wut"}}},

    {__LINE__, "tracestate from subsequent style",
     {PropagationStyle::DATADOG, PropagationStyle::W3C},
     {PropagationStyle::W3C},
     {{"x-datadog-trace-id", "48"}, {"x-datadog-parent-id", "64"},
      {"x-datadog-origin", "Kansas"}, {"x-datadog-sampling-priority", "2"},
      {"traceparent", "00-00000000000000000000000000000030-0000000000000040-01"},
      {"tracestate", "competitor=stuff,dd=o:Nebraska;s:1;ah:choo"}}, // origin is different
     {{"traceparent", "00-00000000000000000000000000000030-000000000000002a-01"},
      {"tracestate", "dd=s:2;p:000000000000002a;o:Kansas;ah:choo,competitor=stuff"}}},

    {__LINE__, "ignore interlopers",
     {PropagationStyle::DATADOG, PropagationStyle::B3, PropagationStyle::W3C},
     {PropagationStyle::W3C},
     {{"x-datadog-trace-id", "48"}, {"x-datadog-parent-id", "64"},
      {"x-datadog-origin", "Kansas"}, {"x-datadog-sampling-priority", "2"},
      {"x-b3-traceid", "00000000000000000000000000000030"},
      {"x-b3-parentspanid", "000000000000002a"},
      {"x-b3-sampled", "0"}, // sampling is different
      {"traceparent", "00-00000000000000000000000000000030-0000000000000040-01"},
      {"tracestate", "competitor=stuff,dd=o:Nebraska;s:1;ah:choo"}},
     {{"traceparent", "00-00000000000000000000000000000030-000000000000002a-01"},
      {"tracestate", "dd=s:2;p:000000000000002a;o:Kansas;ah:choo,competitor=stuff"}}},

    {__LINE__, "don't take tracestate if trace ID doesn't match",
     {PropagationStyle::DATADOG, PropagationStyle::W3C},
     {PropagationStyle::W3C},
     {{"x-datadog-trace-id", "48"}, {"x-datadog-parent-id", "64"},
      {"x-datadog-origin", "Kansas"}, {"x-datadog-sampling-priority", "2"},
      {"traceparent", "00-00000000000000000000000000000031-0000000000000040-01"},
      {"tracestate", "competitor=stuff,dd=o:Nebraska;s:1;ah:choo"}},
     {{"traceparent", "00-00000000000000000000000000000030-000000000000002a-01"},
      {"tracestate", "dd=s:2;p:000000000000002a;o:Kansas"}}},

    {__LINE__, "don't take tracestate if W3C extraction isn't configured",
     {PropagationStyle::DATADOG, PropagationStyle::B3},
     {PropagationStyle::W3C},
     {{"x-datadog-trace-id", "48"}, {"x-datadog-parent-id", "64"},
      {"x-datadog-origin", "Kansas"}, {"x-datadog-sampling-priority", "2"},
      {"traceparent", "00-00000000000000000000000000000030-0000000000000040-01"},
      {"tracestate", "competitor=stuff,dd=o:Nebraska;s:1;ah:choo"}},
     {{"traceparent", "00-00000000000000000000000000000030-000000000000002a-01"},
      {"tracestate", "dd=s:2;p:000000000000002a;o:Kansas"}}},
  }));
  // clang-format on

  CAPTURE(test_case.line);
  CAPTURE(test_case.description);
  CAPTURE(to_json(test_case.extraction_styles));
  CAPTURE(to_json(test_case.injection_styles));
  CAPTURE(test_case.extracted_headers);
  CAPTURE(test_case.expected_injected_headers);

  TracerConfig config;
  config.service = "testsvc";
  config.extraction_styles = test_case.extraction_styles;
  config.injection_styles = test_case.injection_styles;
  config.logger = std::make_shared<NullLogger>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer{*finalized_config, std::make_shared<MockIDGenerator>()};

  MockDictReader reader{test_case.extracted_headers};
  auto span = tracer.extract_span(reader);
  REQUIRE(span);

  MockDictWriter writer;
  span->inject(writer);

  REQUIRE(writer.items == test_case.expected_injected_headers);
}

TEST_CASE("move semantics") {
  // Verify that `Tracer` can be moved.
  TracerConfig config;
  config.service = "testsvc";
  config.logger = std::make_shared<NullLogger>();
  config.collector = std::make_shared<MockCollector>();

  auto finalized_config = finalize_config(config);
  REQUIRE(finalized_config);
  Tracer tracer1{*finalized_config};

  // This must compile.
  Tracer tracer2{std::move(tracer1)};
  (void)tracer2;
}
