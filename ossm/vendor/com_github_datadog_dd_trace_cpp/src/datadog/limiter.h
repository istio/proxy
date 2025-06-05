#pragma once

// This component provides a `class`, `Limiter`, that is an implementation of
// the [token bucket][1] rate limiter.
//
// `Limiter` is used by the `TraceSampler` and the `SpanSampler` to enforce
// their respective `max_per_second` configuration parameters.
//
// [1]: https://en.wikipedia.org/wiki/Token_bucket

#include <vector>

#include "clock.h"
#include "rate.h"

namespace datadog {
namespace tracing {

class Limiter {
 public:
  struct Result {
    bool allowed;
    Rate effective_rate;
  };

  Limiter(const Clock& clock, int max_tokens, double refresh_rate,
          int tokens_per_refresh);
  Limiter(const Clock& clock, double allowed_per_second);

  Result allow();
  Result allow(int tokens);

 private:
  Clock clock_;
  int num_tokens_;
  int max_tokens_;
  std::chrono::steady_clock::duration refresh_interval_;
  int tokens_per_refresh_;
  std::chrono::steady_clock::time_point next_refresh_;
  // effective rate fields
  std::vector<double> previous_rates_;
  double previous_rates_sum_;
  std::chrono::steady_clock::time_point current_period_;
  int num_allowed_ = 0;
  int num_requested_ = 0;
};

}  // namespace tracing
}  // namespace datadog
