// Copyright 2017, OpenCensus Authors
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

#ifndef OPENCENSUS_STATS_DISTRIBUTION_H_
#define OPENCENSUS_STATS_DISTRIBUTION_H_

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "opencensus/stats/bucket_boundaries.h"

namespace opencensus {
namespace stats {

// A Distribution object holds a summary of a stream of double values (e.g. all
// values for one measure and set of tags). It stores both a statistical summary
// (mean, sum of squared deviation, and range) and a histogram recording the
// number of values in each bucket (as defined by a BucketBoundaries).
// This corresponds to a Stackdriver Distribution metric
// (https://cloud.google.com/monitoring/api/ref_v3/rest/v3/TypedValue#Distribution).
// Distribution is thread-compatible.
class Distribution final {
 public:
  const std::vector<uint64_t>& bucket_counts() const { return bucket_counts_; }

  uint64_t count() const { return count_; }
  double mean() const { return mean_; }
  double sum_of_squared_deviation() const { return sum_of_squared_deviation_; }
  double min() const { return min_; }
  double max() const { return max_; }

  const BucketBoundaries& bucket_boundaries() const { return *buckets_; }

  // A string representation of the Distribution's data suitable for human
  // consumption.
  std::string DebugString() const;

 private:
  friend class ViewDataImpl;  // ViewDataImpl populates data directly.
  friend class MeasureData;

  // buckets must outlive the Distribution.
  explicit Distribution(const BucketBoundaries* buckets);

  // Adds 'value' to the distribution. 'value' does not need to be finite, but
  // non-finite values may make statistics meaningless.
  void Add(double value);

  const BucketBoundaries* const buckets_;  // Never null; not owned.

  uint64_t count_ = 0;
  double mean_ = 0;
  double sum_of_squared_deviation_ = 0;
  double min_ = std::numeric_limits<double>::infinity();
  double max_ = -std::numeric_limits<double>::infinity();

  // The counts of values in the buckets listed in buckets_. Size is
  // buckets_->num_buckets().
  std::vector<uint64_t> bucket_counts_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_DISTRIBUTION_H_
