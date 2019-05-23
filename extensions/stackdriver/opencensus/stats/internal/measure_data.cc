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

#include "opencensus/stats/internal/measure_data.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "absl/base/macros.h"
#include "absl/types/span.h"
#include "opencensus/stats/bucket_boundaries.h"
#include "opencensus/stats/distribution.h"

namespace opencensus {
namespace stats {

MeasureData::MeasureData(absl::Span<const BucketBoundaries> boundaries)
    : boundaries_(boundaries) {
  histograms_.reserve(boundaries_.size());
  for (const auto& b : boundaries_) {
    histograms_.emplace_back(b.num_buckets());
  }
}

void MeasureData::Add(double value) {
  last_value_ = value;
  // Update using the method of provisional means.
  ++count_;
  const double old_mean = mean_;
  mean_ += (value - mean_) / count_;
  sum_of_squared_deviation_ =
      sum_of_squared_deviation_ + (value - old_mean) * (value - mean_);

  min_ = std::min(value, min_);
  max_ = std::max(value, max_);

  for (uint32_t i = 0; i < boundaries_.size(); ++i) {
    ++histograms_[i][boundaries_[i].BucketForValue(value)];
  }
}

void MeasureData::AddToDistribution(Distribution* distribution) const {
  AddToDistribution(distribution->bucket_boundaries(), &distribution->count_,
                    &distribution->mean_,
                    &distribution->sum_of_squared_deviation_,
                    &distribution->min_, &distribution->max_,
                    absl::Span<uint64_t>(distribution->bucket_counts_));
}

template <typename T>
void MeasureData::AddToDistribution(const BucketBoundaries& boundaries,
                                    T* count, double* mean,
                                    double* sum_of_squared_deviation,
                                    double* min, double* max,
                                    absl::Span<T> histogram_buckets) const {
  // This uses the method of provisional means generalized for multiple values
  // in both datasets.
  const double new_count = *count + count_;
  const double new_mean = *mean + (mean_ - *mean) * count_ / new_count;
  *sum_of_squared_deviation +=
      sum_of_squared_deviation_ + *count * std::pow(*mean, 2) +
      count_ * std::pow(mean_, 2) - new_count * std::pow(new_mean, 2);
  *count = new_count;
  *mean = new_mean;

  if (*count == count_) {
    // Overwrite in case the destination was zero-initialized.
    *min = min_;
    *max = max_;
  } else {
    *min = std::min(*min, min_);
    *max = std::max(*max, max_);
  }

  uint32_t histogram_index =
      std::find(boundaries_.begin(), boundaries_.end(), boundaries) -
      boundaries_.begin();
  if (histogram_index >= histograms_.size()) {
    // Add to the underflow bucket, to avoid downstream errors from the sum of
    // bucket counts not matching the total count.
    histogram_buckets[0] += count_;
  } else {
    for (uint32_t i = 0; i < histograms_[histogram_index].size(); ++i) {
      histogram_buckets[i] += histograms_[histogram_index][i];
    }
  }
}

template void MeasureData::AddToDistribution(const BucketBoundaries&, double*,
                                             double*, double*, double*, double*,
                                             absl::Span<double>) const;

}  // namespace stats
}  // namespace opencensus
