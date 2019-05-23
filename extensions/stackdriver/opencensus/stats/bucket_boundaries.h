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

#ifndef OPENCENSUS_STATS_BUCKET_BOUNDARIES_H_
#define OPENCENSUS_STATS_BUCKET_BOUNDARIES_H_

#include <string>
#include <utility>
#include <vector>

namespace opencensus {
namespace stats {

// BucketBoundaries defines the bucket boundaries for distribution
// aggregations.
// BucketBoundaries is a value type, and is thread-compatible.
class BucketBoundaries final {
 public:
  // Creates a BucketBoundaries with num_finite_buckets each 'width' wide,
  // as well as an underflow and overflow bucket. 'offset' is the lower bound of
  // the first finite bucket, so finite bucket i (1 <= i <= num_finite_buckets)
  // covers the interval [offset + (i - 1) * width, offset + i * width). The
  // underflow bucket covers [-inf, offset) and the overflow bucket [offset +
  // num_finite_buckets * width, inf].
  static BucketBoundaries Linear(int num_finite_buckets, double offset,
                                 double width);

  // Creates a BucketBoundaries with num_finite_buckets with exponentially
  // increasing boundaries starting at zero (governed by growth_factor and
  // scale), as well as an underflow and overflow bucket. Finite bucket i (1 <=
  // i <= num_finite_buckets) covers the interval [scale * growth_factor ^ (i -
  // 1), scale * growth_factor ^ i). The underflow bucket covers [-inf, 0) and
  // the overflow bucket [scale * growth_factor ^ num_finite_buckets, inf].
  static BucketBoundaries Exponential(int num_finite_buckets, double scale,
                                      double growth_factor);

  // Creates a BucketBoundaries from a monotonically increasing list of
  // boundaries. This will create a bucket covering each interval of
  // [boundaries[i], boundaries[i+1]), as well as an underflow bucket covering
  // [-inf, boundaries[0]) and an overflow bucket covering
  // [boundaries[boundaries.size()-1], inf].
  static BucketBoundaries Explicit(std::vector<double> boundaries);

  // The number of buckets in a Distribution using this bucketer.
  int num_buckets() const { return lower_boundaries_.size() + 1; }
  // The index of the bucket for a given value, in [0, num_buckets() - 1].
  int BucketForValue(double value) const;

  const std::vector<double>& lower_boundaries() const {
    return lower_boundaries_;
  }

  std::string DebugString() const;

  bool operator==(const BucketBoundaries& other) const {
    return lower_boundaries_ == other.lower_boundaries_;
  }
  bool operator!=(const BucketBoundaries& other) const {
    return !(*this == other);
  }

 private:
  BucketBoundaries(std::vector<double> lower_boundaries)
      : lower_boundaries_(std::move(lower_boundaries)) {}

  // The lower bound of each bucket, excluding the underflow bucket but
  // including the overflow bucket.
  std::vector<double> lower_boundaries_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_BUCKET_BOUNDARIES_H_
