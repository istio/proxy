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
#include "null_logger.h"
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
using namespace std::chrono_literals;

#define TEST_TRACER(x) TEST_CASE(x, "[tracer]")

// Verify that the `.defaults.*` (`SpanDefaults`) properties of a tracer's
// configuration do determine the default properties of spans created by the
// tracer.
TEST_TRACER("tracer span defaults") {
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

  // Test behaviors when the config overrides the service but leaves other
  // fields empty
  SpanConfig overrides_with_empty_values;
  overrides_with_empty_values.service = "barsvc";

  REQUIRE(overrides_with_empty_values.service != config.service);
  REQUIRE(overrides_with_empty_values.service_type != config.service_type);
  REQUIRE(overrides_with_empty_values.environment != config.environment);
  REQUIRE(overrides_with_empty_values.version != config.version);
  REQUIRE(overrides_with_empty_values.name != config.name);
  REQUIRE(overrides_with_empty_values.tags != config.tags);

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

    REQUIRE(root.tags.count(tags::version) == 1);
    REQUIRE(root.tags.find(tags::version)->second == config.version);
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

    REQUIRE(root.tags.count(tags::version) == 1);
    REQUIRE(root.tags.find(tags::version)->second == overrides.version);
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

    REQUIRE(span.tags.count(tags::version) == 1);
    REQUIRE(span.tags.find(tags::version)->second == config.version);
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

    REQUIRE(span.tags.count(tags::version) == 1);
    REQUIRE(span.tags.find(tags::version)->second == overrides.version);
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

    REQUIRE(child.tags.count(tags::version) == 1);
    REQUIRE(child.tags.find(tags::version)->second == config.version);
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

    REQUIRE(child.tags.count(tags::version) == 1);
    REQUIRE(child.tags.find(tags::version)->second == overrides.version);
  }

  SECTION("can be overridden in a child span with empty values") {
    {
      auto parent = tracer.create_span();
      parent.create_child(overrides_with_empty_values);
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

    REQUIRE(child.service ==
            overrides_with_empty_values.service);  // only service is set
    REQUIRE(child.service_type == config.service_type);
    REQUIRE(child.environment() == config.environment);
    REQUIRE(child.version() == nullopt);  // version is not inherited since the
                                          // service name is different
    REQUIRE(child.name == config.name);
    REQUIRE_THAT(child.tags, ContainsSubset(*config.tags));

    REQUIRE(child.tags.count(tags::version) == 0);
  }
}

TEST_TRACER("span extraction") {
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
    REQUIRE(!span.parent_id());
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
      auto result2 = tracer.extract_span(reader);
      if (test_case.expected_error) {
        REQUIRE(!result2);
        REQUIRE(result2.error().code == test_case.expected_error);
      } else {
        REQUIRE(result2);
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
         "datadog style with leading and trailing spaces",
         {PropagationStyle::DATADOG},
         {{"x-datadog-trace-id", "   123  "},
          {"x-datadog-parent-id", " 456  "},
          {"x-datadog-sampling-priority", "    2 "}},
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
         "B3 style with leading and trailing spaces",
         {PropagationStyle::B3},
         {{"x-b3-traceid", "   abc   "},
          {"x-b3-spanid", " def  "},
          {"x-b3-sampled", "     0  "}},
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
      checks(test_case, span);
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

        {__LINE__, "valid: w3.org example 1 with leading and trailing spaces",
         "   00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01   ", // traceparent
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
         "malformed_traceid"}, // expected_error_tag_value

        {__LINE__, "invalid: parent ID zero",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-00", // traceparent
         "malformed_parentid"}, // expected_error_tag_value

        {__LINE__, "invalid: trailing characters when version is zero",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-00-foo", // traceparent
         "malformed_traceparent"}, // expected_error_tag_value
      
        {__LINE__, "invalid: non hex trace ID",
         "00-abcdefghijklmnopqrstuvxyzabcdefg-00f067aa0ba902b7-00", // traceparent
         "malformed_traceid"}, // expected_error_tag_value

        {__LINE__, "invalid: non hex parent ID",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-abcdefghijklmnop-00", // traceparent
         "malformed_parentid"}, // expected_error_tag_value

        {__LINE__, "invalid: non hex trace tag ID",
         "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-xy", // traceparent
         "malformed_traceflags"}, // expected_error_tag_value

        {
            __LINE__,
            "invalid: non supported character in trace version 1/x",
            ".0-12345678901234567890123456789012-1234567890123456-01",
            "invalid_version",
        },
        {
            __LINE__,
            "invalid: non supported character in trace version 2/x",
            "0.-12345678901234567890123456789012-1234567890123456-01",
            "invalid_version"
        },
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
            {
                {"_dd.p.foo", "thing1"},
                {"_dd.p.bar", "thing2"},
            },                   // expected_trace_tags
            nullopt,             // expected_additional_w3c_tracestate
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

        {
            __LINE__,
            "invalid trace state (1/2)",
            traceparent_keep,
            "dd=ts:0001",
            1,
            nullopt,
            {},
            nullopt,
            nullopt,
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "invalid trace state (2/2)",
            traceparent_keep,
            "dd=ts:AA",
            1,
            nullopt,
            {},
            nullopt,
            nullopt,
            "0000000000000000",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__,
            "valid trace state",
            traceparent_keep,
            "dd=o:dsm;ts:04",
            1,
            "dsm",
            {
                {"_dd.p.ts", "04"},
            },
            nullopt,
            nullopt,
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

  SECTION("W3C Phase 3 support - Preferring tracecontext") {
    // Tests behavior from system-test
    // test_headers_tracecontext.py::test_tracestate_w3c_p_extract_datadog_w3c
    struct TestCase {
      int line;
      std::string name;
      std::string traceparent;
      Optional<std::string> tracestate;
      Optional<std::string> dd_trace_id;
      Optional<std::string> dd_parent_id;
      Optional<std::string> dd_tags;
      Optional<std::uint64_t> expected_parent_id;
      Optional<std::string> expected_datadog_w3c_parent_id = {};
    };

    auto test_case = GENERATE(values<TestCase>({
        {
            __LINE__, "identical trace info",
            "00-11111111111111110000000000000001-000000003ade68b1-"
            "01",                               // traceparent
            "dd=s:2;p:000000003ade68b1,foo=1",  // tracestate
            "1",                                // x-datadog-trace-id
            "987654321",                        // x-datadog-parent-id
            "_dd.p.tid=1111111111111111",       // x-datadog-tags
            987654321,                          // expected_parent_id
            nullopt,  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__, "trace ids do not match",
            "00-11111111111111110000000000000002-000000003ade68b1-"
            "01",                               // traceparent
            "dd=s:2;p:000000000000000a,foo=1",  // tracestate
            "2",                                // x-datadog-trace-id
            "10",                               // x-datadog-parent-id
            "_dd.p.tid=2222222222222222",       // x-datadog-tags
            10,                                 // expected_parent_id
            nullopt,  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__, "same trace, non-matching parent ids",
            "00-11111111111111110000000000000003-000000003ade68b1-"
            "01",                               // traceparent
            "dd=s:2;p:000000000000000a,foo=1",  // tracestate
            "3",                                // x-datadog-trace-id
            "10",                               // x-datadog-parent-id
            "_dd.p.tid=1111111111111111",       // x-datadog-tags
            987654321,                          // expected_parent_id
            "000000000000000a",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__, "non-matching span, missing p value",
            "00-00000000000000000000000000000004-000000003ade68b1-"
            "01",                // traceparent
            "dd=s:2,foo=1",      // tracestate
            "4",                 // x-datadog-trace-id
            "10",                // x-datadog-parent-id
            nullopt,             // x-datadog-tags
            987654321,           // expected_parent_id
            "000000000000000a",  // expected_datadog_w3c_parent_id,
        },

        {
            __LINE__, "non-matching span, non-matching p value",
            "00-00000000000000000000000000000005-000000003ade68b1-"
            "01",                               // traceparent
            "dd=s:2;p:8fffffffffffffff,foo=1",  // tracestate
            "5",                                // x-datadog-trace-id
            "10",                               // x-datadog-parent-id
            nullopt,                            // x-datadog-tags
            987654321,                          // expected_parent_id
            "8fffffffffffffff",  // expected_datadog_w3c_parent_id,
        },
    }));

    CAPTURE(test_case.name);
    CAPTURE(test_case.line);
    CAPTURE(test_case.traceparent);
    CAPTURE(test_case.tracestate);
    CAPTURE(test_case.dd_trace_id);
    CAPTURE(test_case.dd_parent_id);
    CAPTURE(test_case.dd_tags);

    std::vector<PropagationStyle> extraction_styles{
        PropagationStyle::DATADOG, PropagationStyle::B3, PropagationStyle::W3C};
    config.extraction_styles = extraction_styles;

    auto valid_config = finalize_config(config);
    REQUIRE(valid_config);
    Tracer tracer{*valid_config};

    std::unordered_map<std::string, std::string> headers;
    headers["traceparent"] = test_case.traceparent;
    if (test_case.tracestate) {
      headers["tracestate"] = *test_case.tracestate;
    }
    if (test_case.dd_trace_id) {
      headers["x-datadog-trace-id"] = *test_case.dd_trace_id;
    }
    if (test_case.dd_parent_id) {
      headers["x-datadog-parent-id"] = *test_case.dd_parent_id;
    }
    if (test_case.dd_tags) {
      headers["x-datadog-tags"] = *test_case.dd_tags;
    }
    MockDictReader reader{headers};

    const auto span = tracer.extract_span(reader);
    REQUIRE(span);

    REQUIRE(span->parent_id() == test_case.expected_parent_id);
    REQUIRE(span->lookup_tag(tags::internal::w3c_parent_id) ==
            test_case.expected_datadog_w3c_parent_id);
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
}

TEST_TRACER("baggage usage") {
  TracerConfig config;
  config.logger = std::make_shared<NullLogger>();
  config.collector = std::make_shared<NullCollector>();

  SECTION("disabling baggage propagation yield an error") {
    config.extraction_styles = {PropagationStyle::DATADOG};
    config.injection_styles = {PropagationStyle::DATADOG};

    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);

    Tracer tracer(*finalized_config);

    MockDictReader reader;
    auto maybe_baggage = tracer.extract_baggage(reader);
    CHECK(!maybe_baggage);

    auto baggage = tracer.create_baggage();
    MockDictWriter writer;
    CHECK(!tracer.inject(baggage, writer));
  }

  SECTION("feature is enabled") {
    auto finalized_config = finalize_config(config);
    REQUIRE(finalized_config);

    Tracer tracer(*finalized_config);

    MockDictReader reader;
    auto baggage = tracer.extract_or_create_baggage(reader);

    baggage.set("data", "dog");
    MockDictWriter writer;
    tracer.inject(baggage, writer);

    REQUIRE(writer.items.count("baggage") == 1);
    CHECK(writer.items["baggage"] == "data=dog");
  }
}

TEST_TRACER("report hostname") {
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

TEST_TRACER("128-bit trace IDs") {
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

TEST_TRACER(
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

TEST_TRACER("heterogeneous extraction") {
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
  CAPTURE(test_case.extraction_styles);
  CAPTURE(test_case.injection_styles);
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

TEST_TRACER("move semantics") {
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

TEST_TRACER("APM tracing disabled") {
  TracerConfig config;
  config.service = "testsvc";
  config.name = "test.op";
  auto collector = std::make_shared<MockCollector>();
  config.collector = collector;
  config.logger = std::make_shared<NullLogger>();
  config.tracing_enabled = false;

  TimePoint current_time = default_clock();
  auto clock = [&current_time]() { return current_time; };

  SECTION("sampling behaviour") {
    SECTION("span with _dd.p.ts is kept") {
      auto finalized_config = finalize_config(config, clock);
      REQUIRE(finalized_config);
      REQUIRE(!finalized_config->tracing_enabled);
      Tracer tracer{*finalized_config};

      {
        auto span = tracer.create_span();
        span.set_source(Source::appsec);
      }

      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);
      const datadog::tracing::SpanData& span_data =
          *collector->chunks.front().front();

      CHECK(span_data.tags.at("_dd.p.dm") == "-5");
      CHECK(span_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
      CHECK(span_data.numeric_tags.at(tags::internal::sampling_priority) == 2);
    }

    SECTION("spans without _dd.p.ts are rate limited to 1/min") {
      auto finalized_config = finalize_config(config, clock);
      REQUIRE(finalized_config);
      Tracer tracer{*finalized_config};
      { auto root1 = tracer.create_span(); }
      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);

      {
        const datadog::tracing::SpanData& span1_data =
            *collector->chunks.front().front();
        CHECK(span1_data.numeric_tags.at(tags::internal::sampling_priority) ==
              2);
        CHECK(span1_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
        CHECK(span1_data.tags.at("_dd.p.dm") == "-0");
      }

      collector->chunks.clear();

      {
        current_time += 1s;  // Advance clock a bit, still within 1 min window
        tracer.create_span();
      }

      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);

      // Expect the span to be dropped because we already ingested 1 trace in
      // the current 1 min window.
      {
        const datadog::tracing::SpanData& span2_data =
            *collector->chunks.front().front();
        CHECK(span2_data.numeric_tags.at(tags::internal::sampling_priority) ==
              -1);
        CHECK(span2_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
      }

      collector->chunks.clear();

      {
        auto span = tracer.create_span();
        span.set_source(Source::appsec);
      }

      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);

      // Expect the span to be kept because the trace source is set.
      {
        const datadog::tracing::SpanData& span2_data =
            *collector->chunks.front().front();
        CHECK(span2_data.numeric_tags.at(tags::internal::sampling_priority) ==
              2);
        CHECK(span2_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
      }

      collector->chunks.clear();

      {
        current_time += 1min + 1s;  // Advance clock past 1 min
        tracer.create_span();
      }

      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);

      // Expect to be ingested.
      {
        const auto& span3_data = *collector->chunks.front().front();
        CHECK(span3_data.numeric_tags.at(tags::internal::sampling_priority) ==
              2);
        CHECK(span3_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
      }
    }
  }

  SECTION("extracted context behavior") {
    auto finalized_config = finalize_config(config, clock);
    REQUIRE(finalized_config);
    Tracer tracer{*finalized_config};

    // When APM Tracing is disabled, we allow one trace per second for service
    // liveness. To ensure consistency, consume the limiter slot.
    { tracer.create_span(); }
    collector->chunks.clear();

    // Case 1: extracted context with priority, but no `_dd.p.ts` → depends if
    // local spans are marked by a product.
    SECTION("no trace source extracted") {
      const std::unordered_map<std::string, std::string> headers_with_priority{
          {"x-datadog-trace-id", "123"},
          {"x-datadog-parent-id", "456"},
          {"x-datadog-sampling-priority", "2"}  // USER_KEEP
      };

      SECTION(
          "tracer apply its own sampling decision in accordance with the "
          "locally enabled product") {
        {
          MockDictReader reader{headers_with_priority};
          auto span = tracer.extract_span(reader);
          REQUIRE(span);
        }

        REQUIRE(collector->chunks.size() == 1);
        REQUIRE(collector->chunks.front().size() == 1);

        // although incoming priority was USER_KEEP, we should still drop it
        // because we already consumed the only slot from the limiter.
        {
          const SpanData& span_data = *collector->chunks.front().front();
          CHECK(span_data.numeric_tags.at(tags::internal::sampling_priority) ==
                -1);
          CHECK(span_data.numeric_tags.at(tags::internal::apm_enabled) == 0.);
          collector->chunks.clear();
        }

        // Mark the span as generated by the Appsec product. This should ensure
        // the span is retained.
        {
          MockDictReader reader{headers_with_priority};
          auto span = tracer.extract_span(reader);
          REQUIRE(span);
          span->set_source(Source::appsec);
        }

        REQUIRE(collector->chunks.size() == 1);
        REQUIRE(collector->chunks.front().size() == 1);

        {
          const SpanData& span_data = *collector->chunks.front().front();
          CHECK(span_data.numeric_tags.at(tags::internal::sampling_priority) ==
                2);
          CHECK(span_data.tags.at(tags::internal::decision_maker) == "-5");
          CHECK(span_data.tags.at(tags::internal::trace_source) ==
                to_tag(Source::appsec));
          CHECK(span_data.numeric_tags.at(tags::internal::apm_enabled) == 0.);
          collector->chunks.clear();
        }

        // Advance the clock to reset the limiter.
        current_time += 1min + 10s;

        // This span qualifies as the one trace per minute allowed for service
        // liveness, so it will be retained.
        {
          MockDictReader reader{headers_with_priority};
          auto span = tracer.extract_span(reader);
          REQUIRE(span);
        }

        REQUIRE(collector->chunks.size() == 1);
        REQUIRE(collector->chunks.front().size() == 1);

        {
          const SpanData& span_data = *collector->chunks.front().front();
          CHECK(span_data.numeric_tags.at(tags::internal::sampling_priority) ==
                2);
          CHECK(span_data.tags.at(tags::internal::decision_maker) == "-0");
          CHECK(span_data.numeric_tags.at(tags::internal::apm_enabled) == 0.);
          collector->chunks.clear();
        }
      }
    }

    // Case 2: Extracted context with priority AND _dd.p.ts -> Kept by AppSec
    // rule.
    SECTION("trace source is extracted") {
      const std::unordered_map<std::string, std::string>
          headers_with_priority_and_appsec{
              {"x-datadog-trace-id", "789"},
              {"x-datadog-parent-id", "101"},
              // USER_DROP, to show _dd.p.ts overrides
              {"x-datadog-sampling-priority", "-1"},
              {"x-datadog-tags", "_dd.p.ts=02"}};

      {
        MockDictReader reader{headers_with_priority_and_appsec};
        auto span = tracer.extract_span(reader);
        REQUIRE(span);
      }

      REQUIRE(collector->chunks.size() == 1);
      REQUIRE(collector->chunks.front().size() == 1);

      {
        const SpanData& span2_data = *collector->chunks.front().front();
        CHECK(span2_data.numeric_tags.at(tags::internal::sampling_priority) ==
              2);
        CHECK(span2_data.numeric_tags.at(tags::internal::apm_enabled) == 0);
      }
    }
  }
}
