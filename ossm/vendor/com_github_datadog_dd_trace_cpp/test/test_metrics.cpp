// This test covers operations defined for metrics defined in `metrics.h`.

#include <datadog/metrics.h>

#include "test.h"

using namespace datadog::tracing;

TEST_CASE("Counter metrics") {
  CounterMetric metric = {"test.counter.metric", {"testing-testing:123"}, true};

  metric.inc();
  metric.add(41);
  REQUIRE(metric.value() == 42);
  auto captured_value = metric.capture_and_reset_value();
  REQUIRE(captured_value == 42);
  REQUIRE(metric.value() == 0);
}

TEST_CASE("Gauge metrics") {
  GaugeMetric metric = {"test.gauge.metric", {"testing-testing:123"}, true};
  metric.set(40);
  metric.inc();
  metric.add(10);
  metric.sub(8);
  metric.dec();
  REQUIRE(metric.value() == 42);
  auto captured_value = metric.capture_and_reset_value();
  REQUIRE(captured_value == 42);
  REQUIRE(metric.value() == 0);

  metric.add(10);
  metric.sub(11);
  REQUIRE(metric.value() == 0);
}
