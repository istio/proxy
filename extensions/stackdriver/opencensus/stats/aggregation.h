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

#ifndef OPENCENSUS_STATS_AGGREGATION_H_
#define OPENCENSUS_STATS_AGGREGATION_H_

#include <string>
#include <utility>

#include "opencensus/stats/bucket_boundaries.h"

namespace opencensus {
namespace stats {

// Aggregation defines how to aggregate data for each view. See the static
// constructors for details of the various options.
// Aggregation is immutable.
class Aggregation final {
 public:
  // Count aggregation counts the number of records, ignoring their individual
  // values. Note that 'count' measures (e.g. the count of RPCs received) should
  // use Sum() aggregation to correctly handle non-unit recorded values.
  static Aggregation Count() {
    return Aggregation(Type::kCount, BucketBoundaries::Explicit({}));
  }

  // Sum aggregation sums all records.
  static Aggregation Sum() {
    return Aggregation(Type::kSum, BucketBoundaries::Explicit({}));
  }

  // Distribution aggregation calculates distribution statistics (count, mean,
  // range, and sum of squared deviation) and tracks a histogram of recorded
  // values according to 'buckets'.
  static Aggregation Distribution(BucketBoundaries buckets) {
    return Aggregation(Type::kDistribution, std::move(buckets));
  }

  // LastValue aggregation returns the last value recorded.
  static Aggregation LastValue() {
    return Aggregation(Type::kLastValue, BucketBoundaries::Explicit({}));
  }

  enum class Type {
    kCount,
    kSum,
    kDistribution,
    kLastValue,
  };

  Type type() const { return type_; }
  const BucketBoundaries& bucket_boundaries() const {
    return bucket_boundaries_;
  }

  std::string DebugString() const;

  bool operator==(const Aggregation& other) const {
    return type_ == other.type_ &&
           bucket_boundaries_ == other.bucket_boundaries_;
  }
  bool operator!=(const Aggregation& other) const { return !(*this == other); }

 private:
  Aggregation(Type type, BucketBoundaries buckets)
      : type_(type), bucket_boundaries_(std::move(buckets)) {}

  Type type_;
  // Ignored except if type_ == kDistribution.
  BucketBoundaries bucket_boundaries_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_AGGREGATION_H_
