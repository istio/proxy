#include "threaded_event_scheduler.h"

#include <thread>

#include "json.hpp"

namespace datadog {
namespace tracing {

ThreadedEventScheduler::EventConfig::EventConfig(
    std::function<void()> callback,
    std::chrono::steady_clock::duration interval)
    : callback(callback), interval(interval), cancelled(false) {}

bool ThreadedEventScheduler::GreaterThan::operator()(
    const ScheduledRun& left, const ScheduledRun& right) const {
  return left.when > right.when;
}

ThreadedEventScheduler::ThreadedEventScheduler()
    : running_current_(false),
      shutting_down_(false),
      dispatcher_([this]() { run(); }) {}

ThreadedEventScheduler::~ThreadedEventScheduler() {
  {
    std::lock_guard guard(mutex_);
    shutting_down_ = true;
    schedule_or_shutdown_.notify_one();
  }
  dispatcher_.join();
}

EventScheduler::Cancel ThreadedEventScheduler::schedule_recurring_event(
    std::chrono::steady_clock::duration interval,
    std::function<void()> callback) {
  const auto now = std::chrono::steady_clock::now();
  auto config = std::make_shared<EventConfig>(std::move(callback), interval);

  {
    std::lock_guard<std::mutex> guard(mutex_);
    upcoming_.push(ScheduledRun{now + interval, config});
    schedule_or_shutdown_.notify_one();
  }

  // Return a cancellation function.
  return [this, config = std::move(config)]() mutable {
    if (!config) {
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    config->cancelled = true;
    current_done_.wait(lock, [this, &config]() {
      return !running_current_ || current_.config != config;
    });
    config.reset();
  };
}

nlohmann::json ThreadedEventScheduler::config_json() const {
  return nlohmann::json::object(
      {{"type", "datadog::tracing::ThreadedEventScheduler"}});
}

void ThreadedEventScheduler::run() {
  std::unique_lock<std::mutex> lock(mutex_);

  for (;;) {
    schedule_or_shutdown_.wait(
        lock, [this]() { return shutting_down_ || !upcoming_.empty(); });
    if (shutting_down_) {
      return;
    }

    current_ = upcoming_.top();

    if (current_.config->cancelled) {
      upcoming_.pop();
      continue;
    }

    const bool changed =
        schedule_or_shutdown_.wait_until(lock, current_.when, [this]() {
          return shutting_down_ || upcoming_.top().config != current_.config;
        });

    if (shutting_down_) {
      return;
    }

    if (changed) {
      // A more recent event has been scheduled.
      continue;
    }

    // We waited for `current_` and now it's its turn.
    upcoming_.pop();
    if (current_.config->cancelled) {
      continue;
    }

    upcoming_.push(ScheduledRun{current_.when + current_.config->interval,
                                current_.config});
    running_current_ = true;
    lock.unlock();
    current_.config->callback();
    lock.lock();
    running_current_ = false;
    current_done_.notify_all();
  }
}

}  // namespace tracing
}  // namespace datadog
