// Copyright 2018, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENCENSUS_STATS_INTERNAL_AGGREGATION_WINDOW_H_
#define OPENCENSUS_STATS_INTERNAL_AGGREGATION_WINDOW_H_

#include <climits>
#include <string>

namespace opencensus {
namespace stats {

// AggregationWindow defines the time range over which recorded data is
// aggregated for each view.
// AggregationWindow is immutable.
class AggregationWindow final {
 public:
  // Cumulative aggregation accumulates data over the lifetime of the process.
  static AggregationWindow Cumulative() {
    return AggregationWindow(Type::kCumulative, ULLONG_MAX);
  }

  // Delta aggregation accumulates data until it is requested and then resets
  // it, so that each recorded value appears in exactly one delta.
  static AggregationWindow Delta() {
    return AggregationWindow(Type::kDelta, ULLONG_MAX);
  }

  // Interval aggregation keeps a rolling total of usage over the previous
  // 'interval' of time.
  static AggregationWindow Interval(uint64_t interval) {
    return AggregationWindow(Type::kInterval, interval);
  }

  enum class Type {
    kCumulative,
    kDelta,
    kInterval,
  };

  Type type() const { return type_; }
  uint64_t duration() const { return duration_; }

  std::string DebugString() const;

  bool operator==(const AggregationWindow& other) const {
    return type_ == other.type_ && duration_ == other.duration_;
  }
  bool operator!=(const AggregationWindow& other) const {
    return !(*this == other);
  }

 private:
  AggregationWindow(Type type, uint64_t duration)
      : type_(type), duration_(duration) {}

  Type type_;
  // Should always be InfiniteDuration if type_ == kCumulative, to simplify
  // equality checking.
  uint64_t duration_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_INTERNAL_AGGREGATION_WINDOW_H_
