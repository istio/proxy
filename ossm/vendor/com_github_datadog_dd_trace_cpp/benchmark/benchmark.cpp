#include <benchmark/benchmark.h>
#include <datadog/collector.h>
#include <datadog/logger.h>
#include <datadog/span_data.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <datadog/json.hpp>
#include <memory>

#include "hasher.h"

namespace {

namespace dd = datadog::tracing;

// `NullLogger` doesn't log. It avoids `log_startup` spam in the benchmark.
struct NullLogger : public dd::Logger {
  void log_error(const LogFunc&) override {}
  void log_startup(const LogFunc&) override {}
  void log_error(const dd::Error&) override {}
  void log_error(dd::StringView) override {}
};

// `SerializingCollector` immediately MessagePack-serializes spans sent to it.
// This allows us to track the overhead of the serialization code, without
// having to use HTTP as is done in the default collector, `DatadogAgent`.
struct SerializingCollector : public dd::Collector {
  dd::Expected<void> send(
      std::vector<std::unique_ptr<dd::SpanData>>&& spans,
      const std::shared_ptr<dd::TraceSampler>& /*response_handler*/) override {
    std::string buffer;
    return dd::msgpack_encode(buffer, spans);
  }

  nlohmann::json config_json() const override {
    return nlohmann::json::object({{"type", "SerializingCollector"}});
  }
};

// The benchmark `BM_TraceTinyCCSource`, for each iteration over `state`,
// creates a trace whose shape is the same as the file system tree under
// `./tinycc`. It's similar to what is done in `../example`.
void BM_TraceTinyCCSource(benchmark::State& state) {
  for (auto _ : state) {
    dd::TracerConfig config;
    config.service = "benchmark";
    config.logger = std::make_shared<NullLogger>();
    config.collector = std::make_shared<SerializingCollector>();
    const auto valid_config = dd::finalize_config(config);
    dd::Tracer tracer{*valid_config};
    // Note: This assumes that the benchmark is run from the repository root.
    sha256_traced("benchmark/tinycc", tracer);
  }
}
BENCHMARK(BM_TraceTinyCCSource);

}  // namespace

BENCHMARK_MAIN();
