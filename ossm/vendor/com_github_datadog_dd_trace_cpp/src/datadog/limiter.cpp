#include "limiter.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace datadog {
namespace tracing {

Limiter::Limiter(const Clock& clock, int max_tokens, double refresh_rate,
                 int tokens_per_refresh)
    : clock_(clock),
      num_tokens_(max_tokens),
      max_tokens_(max_tokens),
      tokens_per_refresh_(tokens_per_refresh),
      previous_rates_(9, 1.0) {
  // calculate refresh interval: (1/rate) * tokens per refresh as nanoseconds
  refresh_interval_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::seconds(1)) /
                          refresh_rate) *
                      tokens_per_refresh_;

  auto now = clock_().tick;
  next_refresh_ = now + refresh_interval_;
  current_period_ = std::chrono::time_point_cast<std::chrono::seconds>(now);
  previous_rates_sum_ =
      std::accumulate(previous_rates_.begin(), previous_rates_.end(), 0.0);
}

Limiter::Limiter(const Clock& clock, double allowed_per_second)
    : Limiter(clock, int(std::ceil(allowed_per_second)), allowed_per_second,
              1) {}

Limiter::Result Limiter::allow() { return allow(1); }

Limiter::Result Limiter::allow(int tokens_requested) {
  auto now = clock_().tick;

  // update effective rate calculations
  auto intervals = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::time_point_cast<std::chrono::seconds>(now) -
                       current_period_)
                       .count();
  if (intervals > 0) {
    if (std::size_t(intervals) >= previous_rates_.size()) {
      std::fill(previous_rates_.begin() + 1, previous_rates_.end(), 1.0);
    } else {
      std::move_backward(previous_rates_.begin(),
                         previous_rates_.end() - intervals,
                         previous_rates_.end());
      if (num_requested_ > 0) {
        previous_rates_[intervals - 1] =
            double(num_allowed_) / double(num_requested_);
      } else {
        previous_rates_[intervals - 1] = 1.0;
      }
      if (intervals - 2 > 0) {
        std::fill(previous_rates_.begin(),
                  previous_rates_.begin() + intervals - 2, 1.0);
      }
    }
    previous_rates_sum_ =
        std::accumulate(previous_rates_.begin(), previous_rates_.end(), 0.0);
    num_allowed_ = 0;
    num_requested_ = 0;
    current_period_ = now;
  }

  num_requested_++;
  // refill "tokens"
  if (now >= next_refresh_) {
    intervals = (now - next_refresh_).count() / refresh_interval_.count() + 1;
    if (intervals > 0) {
      next_refresh_ += refresh_interval_ * intervals;
      num_tokens_ += static_cast<int>(intervals * tokens_per_refresh_);
      if (num_tokens_ > max_tokens_) {
        num_tokens_ = max_tokens_;
      }
    }
  }
  // determine if allowed or not
  bool allowed = false;
  if (num_tokens_ >= tokens_requested) {
    allowed = true;
    num_allowed_++;
    num_tokens_ -= tokens_requested;
  }

  // `effective_rate` is guaranteed to be between 0.0 and 1.0.
  double effective_rate =
      (previous_rates_sum_ + double(num_allowed_) / double(num_requested_)) /
      (previous_rates_.size() + 1);

  return {allowed, *Rate::from(effective_rate)};
}

}  // namespace tracing
}  // namespace datadog
