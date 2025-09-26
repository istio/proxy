#pragma once

// This component provides an interface, `EventScheduler`, that allows a
// specified function-like object to be invoked at regular intervals.
//
// `DatadogAgent` uses an `EventScheduler` to periodically send batches of
// traces to the Datadog Agent.
//
// The default implementation is `ThreadedEventScheduler`.  See
// `threaded_event_scheduler.h`.

#include <chrono>
#include <functional>

#include "error.h"
#include "json_fwd.hpp"

namespace datadog {
namespace tracing {

class EventScheduler {
 public:
  using Cancel = std::function<void()>;

  // Invoke the specified `callback` repeatedly, with the specified `interval`
  // elapsing between invocations.  The first invocation is after an initial
  // `interval`.  Return a function-like object that can be invoked without
  // arguments to prevent subsequent invocations of `callback`.
  virtual Cancel schedule_recurring_event(
      std::chrono::steady_clock::duration interval,
      std::function<void()> callback) = 0;

  // Return a JSON representation of this object's configuration. The JSON
  // representation is an object with the following properties:
  //
  // - "type" is the unmangled, qualified name of the most-derived class, e.g.
  //   "datadog::tracing::ThreadedEventScheduler".
  // - "config" is an object containing this object's configuration. "config"
  //   may be omitted if the derived class has no configuration.
  virtual nlohmann::json config_json() const = 0;

  virtual ~EventScheduler() = default;
};

}  // namespace tracing
}  // namespace datadog
