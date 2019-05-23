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

#include "opencensus/stats/bucket_boundaries.h"

#include <algorithm>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace opencensus {
namespace stats {

// Class-level todos:
// TODO: Consider lazy generation of storage buckets, to save memory
// when few buckets are populated.
// TODO: Share bucketers, or at least the lower_boundaries_ vector, to
// reduce allocation/copying for copies of Aggregation objects.

// static
BucketBoundaries BucketBoundaries::Linear(int num_finite_buckets, double offset,
                                          double width) {
  std::vector<double> boundaries(num_finite_buckets + 1);
  double boundary = offset;
  for (int i = 0; i <= num_finite_buckets; ++i) {
    boundaries[i] = boundary;
    boundary += width;
  }
  return BucketBoundaries(std::move(boundaries));
}

// static
BucketBoundaries BucketBoundaries::Exponential(int num_finite_buckets,
                                               double scale,
                                               double growth_factor) {
  std::vector<double> boundaries(num_finite_buckets + 1);
  double upper_bound = scale;
  for (int i = 1; i <= num_finite_buckets; ++i) {
    boundaries[i] = upper_bound;
    upper_bound *= growth_factor;
  }
  return BucketBoundaries(std::move(boundaries));
}

// static
BucketBoundaries BucketBoundaries::Explicit(std::vector<double> boundaries) {
  if (!std::is_sorted(boundaries.begin(), boundaries.end())) {
    return BucketBoundaries({});
  }
  return BucketBoundaries(std::move(boundaries));
}

int BucketBoundaries::BucketForValue(double value) const {
  return std::upper_bound(lower_boundaries_.begin(), lower_boundaries_.end(),
                          value) -
         lower_boundaries_.begin();
}

std::string BucketBoundaries::DebugString() const {
  return absl::StrCat("Buckets: ", absl::StrJoin(lower_boundaries_, ","));
}

}  // namespace stats
}  // namespace opencensus
