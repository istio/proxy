#pragma once

#include <datadog/collector.h>
#include <datadog/collector_response.h>
#include <datadog/sampling_priority.h>
#include <datadog/span_data.h>
#include <datadog/tags.h>
#include <datadog/trace_sampler.h>

#include <cstddef>
#include <map>
#include <numeric>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../test.h"

using namespace datadog::tracing;

struct MockCollector : public Collector {
  std::vector<std::vector<std::unique_ptr<SpanData>>> chunks;

  Expected<void> send(std::vector<std::unique_ptr<SpanData>>&& spans,
                      const std::shared_ptr<TraceSampler>&) override {
    chunks.emplace_back(std::move(spans));
    return {};
  }

  nlohmann::json config_json() const override;

  SpanData& first_span() const {
    REQUIRE(chunks.size() >= 1);
    const auto& chunk = chunks.front();
    REQUIRE(chunk.size() >= 1);
    const auto& span_ptr = chunk.front();
    REQUIRE(span_ptr);
    return *span_ptr;
  }

  std::size_t span_count() const {
    return std::accumulate(chunks.begin(), chunks.end(), std::size_t(0),
                           [](std::size_t total, const auto& chunk) {
                             return total + chunk.size();
                           });
  }
};

struct MockCollectorWithResponse : public MockCollector {
  CollectorResponse response;

  Expected<void> send(
      std::vector<std::unique_ptr<SpanData>>&& spans,
      const std::shared_ptr<TraceSampler>& response_handler) override {
    MockCollector::send(std::move(spans), response_handler);
    response_handler->handle_collector_response(response);
    return {};
  }

  nlohmann::json config_json() const override;
};

struct PriorityCountingCollector : public Collector {
  std::map<int, std::size_t> sampling_priority_count;

  Expected<void> send(std::vector<std::unique_ptr<SpanData>>&& spans,
                      const std::shared_ptr<TraceSampler>&) override {
    const SpanData& root = root_span(spans);
    const auto priority =
        root.numeric_tags.at(tags::internal::sampling_priority);
    ++sampling_priority_count[priority];
    return {};
  }

  nlohmann::json config_json() const override;

  const SpanData& root_span(
      const std::vector<std::unique_ptr<SpanData>>& spans) {
    REQUIRE(!spans.empty());

    std::unordered_set<std::uint64_t> span_ids;
    for (const auto& span_ptr : spans) {
      REQUIRE(span_ptr);
      const auto& span = *span_ptr;
      if (span.parent_id == 0) {
        return span;
      }
      span_ids.insert(span.span_id);
    }

    for (const auto& span_ptr : spans) {
      REQUIRE(span_ptr);
      const auto& span = *span_ptr;
      if (span_ids.count(span.parent_id) == 0) {
        // first one wins
        return span;
      }
    }

    // fallback (and actually guaranteed correct as of this writing)
    return *spans.front();
  }

  std::size_t total_count() const {
    return std::accumulate(
        sampling_priority_count.begin(), sampling_priority_count.end(),
        std::size_t(0),
        [](std::size_t sum, const auto& item) { return sum + item.second; });
  }

  std::size_t count_of(SamplingPriority priority) const {
    auto found = sampling_priority_count.find(int(priority));
    if (found != sampling_priority_count.end()) {
      return found->second;
    }
    return 0;
  }

  double ratio_of(SamplingPriority priority) const {
    return double(count_of(priority)) / total_count();
  }
};

struct PriorityCountingCollectorWithResponse
    : public PriorityCountingCollector {
  CollectorResponse response;

  Expected<void> send(
      std::vector<std::unique_ptr<SpanData>>&& spans,
      const std::shared_ptr<TraceSampler>& response_handler) override {
    PriorityCountingCollector::send(std::move(spans), response_handler);
    REQUIRE(response_handler);
    response_handler->handle_collector_response(response);
    return {};
  }

  nlohmann::json config_json() const override;
};

struct FailureCollector : public Collector {
  Error failure{Error::OTHER, "send(...) failed because I told it to."};

  Expected<void> send(std::vector<std::unique_ptr<SpanData>>&&,
                      const std::shared_ptr<TraceSampler>&) override {
    return failure;
  }

  nlohmann::json config_json() const override;
};
