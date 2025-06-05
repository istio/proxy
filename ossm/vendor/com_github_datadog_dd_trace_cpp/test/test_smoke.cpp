#include <datadog/null_collector.h>
#include <datadog/span.h>
#include <datadog/span_config.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

TEST_CASE("smoke") {
  TracerConfig config;
  config.service = "testsvc";
  config.logger = std::make_shared<NullLogger>();
  config.collector = std::make_shared<NullCollector>();

  auto maybe_config = finalize_config(config);
  REQUIRE(maybe_config);

  Tracer tracer{*maybe_config};
  SpanConfig span_config;
  span_config.name = "do.thing";
  Span root = tracer.create_span(span_config);

  span_config.name = "another.thing";
  Span child = root.create_child(span_config);
}
