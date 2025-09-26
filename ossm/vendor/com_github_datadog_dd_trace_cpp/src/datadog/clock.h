#pragma once

// This component provides wrappers around some `std::chrono` vocabulary types:
// `Duration` for an interval of time, and `TimePoint` for an instant in time.
//
// Each `Span` has a start time and a duration. The start time ought to be
// measured using a system clock, so that Network Time Protocol adjustments and
// other time settings are accurately reflected in the span start time.  The
// span's duration, however, is better measured using a steady (monotonic) clock
// so that adjustments to the system clock made during the extent of the span do
// not skew the span's measured duration.
//
// `Duration` is an alias for `std::chrono::steady_clock::duration`, while
// `struct TimePoint` contains two `time_point` values: one from the system
// clock, used for the start time, and another from the steady (monotonic)
// clock, used for determining span duration.
//
// `Clock` is an alias for `std::function<TimePoint()>`, and the default
// `Clock`, `default_clock`, gives a `TimePoint` using the
// `std::chrono::system_clock` and `std::chrono::steady_clock`.

#include <chrono>
#include <functional>

namespace datadog {
namespace tracing {

using Duration = std::chrono::steady_clock::duration;

struct TimePoint {
  std::chrono::system_clock::time_point wall =
      std::chrono::system_clock::time_point();
  std::chrono::steady_clock::time_point tick =
      std::chrono::steady_clock::time_point();
};

inline Duration operator-(const TimePoint& after, const TimePoint& before) {
  return after.tick - before.tick;
}

inline TimePoint operator-(const TimePoint& origin, Duration offset) {
  return {origin.wall -
              std::chrono::duration_cast<std::chrono::system_clock::duration>(
                  offset),
          origin.tick - offset};
}

inline TimePoint& operator+=(TimePoint& self, Duration offset) {
  self.wall +=
      std::chrono::duration_cast<std::chrono::system_clock::duration>(offset);
  self.tick += offset;
  return self;
}

using Clock = std::function<TimePoint()>;

extern const Clock default_clock;

}  // namespace tracing
}  // namespace datadog
