#pragma once

// This component provides a `class`, `NullCollector`, that implements the
// `Collector` interface in terms of a no-op. It's used in unit tests.

#include "collector.h"

namespace datadog {
namespace tracing {

class NullCollector : public Collector {
 public:
  Expected<void> send(std::vector<std::unique_ptr<SpanData>>&&,
                      const std::shared_ptr<TraceSampler>&) override {
    return {};
  }

  std::string config() const override {
    // clang-format off
    return R"({
        "type": "datadog::tracing::NullCollector",
        "config": {}
    })";
    // clang-format on
  };
};

}  // namespace tracing
}  // namespace datadog
