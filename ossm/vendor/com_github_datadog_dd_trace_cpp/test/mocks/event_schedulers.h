#pragma once

#include <datadog/event_scheduler.h>
#include <datadog/optional.h>

#include <chrono>
#include <datadog/json.hpp>
#include <functional>

using namespace datadog::tracing;

struct MockEventScheduler : public EventScheduler {
  std::function<void()> event_callback;
  Optional<std::chrono::steady_clock::duration> recurrence_interval;
  bool cancelled = false;

  Cancel schedule_recurring_event(std::chrono::steady_clock::duration interval,
                                  std::function<void()> callback) override {
    event_callback = callback;
    recurrence_interval = interval;
    return [this]() { cancelled = true; };
  }

  nlohmann::json config_json() const override {
    return nlohmann::json::object({{"type", "MockEventScheduler"}});
  }
};
