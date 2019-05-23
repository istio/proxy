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

#include "opencensus/stats/distribution.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace opencensus {
namespace stats {

Distribution::Distribution(const BucketBoundaries* buckets)
    : buckets_(buckets), bucket_counts_(buckets->num_buckets()) {}

void Distribution::Add(double value) {
  // Update using the method of provisional means.
  ++count_;
  const double new_mean = mean_ + (value - mean_) / count_;
  sum_of_squared_deviation_ =
      sum_of_squared_deviation_ + (value - mean_) * (value - new_mean);
  mean_ = new_mean;

  min_ = std::min(value, min_);
  max_ = std::max(value, max_);

  ++bucket_counts_[buckets_->BucketForValue(value)];
}

std::string Distribution::DebugString() const {
  return absl::StrCat("count: ", count_, " mean: ", mean_,
                      " sum of squared deviation: ", sum_of_squared_deviation_,
                      " min: ", min_, " max: ", max_, "\nhistogram counts: ",
                      absl::StrJoin(bucket_counts_, ", "));
}

}  // namespace stats
}  // namespace opencensus
