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

#ifndef OPENCENSUS_STATS_INTERNAL_DELTA_PRODUCER_H_
#define OPENCENSUS_STATS_INTERNAL_DELTA_PRODUCER_H_

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "opencensus/stats/bucket_boundaries.h"
#include "opencensus/stats/distribution.h"
#include "opencensus/stats/internal/measure_data.h"
#include "opencensus/stats/measure.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace stats {

// Delta is thread-compatible.
class Delta final {
 public:
  void Record(std::initializer_list<Measurement> measurements,
              opencensus::tags::TagMap tags);

  // Swaps registered_boundaries_ and delta_ with *other, clears delta_, and
  // updates registered_boundaries_.
  void SwapAndReset(
      std::vector<std::vector<BucketBoundaries>>& registered_boundaries,
      Delta* other);

  // Clears registered_boundaries_ and delta_.
  void clear();

  const std::unordered_map<opencensus::tags::TagMap, std::vector<MeasureData>,
                           opencensus::tags::TagMap::Hash>&
  delta() const {
    return delta_;
  }

 private:
  // A copy of registered_boundaries_ in the DeltaProducer as of when the
  // delta was started.
  std::vector<std::vector<BucketBoundaries>> registered_boundaries_;

  // The actual data. Each MeasureData[] contains one element for each
  // registered measure.
  std::unordered_map<opencensus::tags::TagMap, std::vector<MeasureData>,
                     opencensus::tags::TagMap::Hash>
      delta_;
};

// DeltaProducer is thread-safe.
class DeltaProducer final {
 public:
  // Returns a pointer to the singleton DeltaProducer.
  static DeltaProducer* Get();

  // Adds a new Measure.
  void AddMeasure();

  // Adds a new BucketBoundaries for the measure 'index' if it does not already
  // exist.
  void AddBoundaries(uint64_t index, const BucketBoundaries& boundaries);

  void Record(std::initializer_list<Measurement> measurements,
              opencensus::tags::TagMap tags);

  // Flushes the active delta and blocks until it is harvested.
  // Returns true if there is measurement added since last flush, returns false
  // if no measurement is added.
  bool Flush();

 private:
  DeltaProducer();

  // Flushing has two stages: swapping active_delta_ to last_delta_ and
  // consuming last_delta_. Callers should release delta_mu_ before calling
  // ConsumeLastDelta so that Record() is blocked for as little time as
  // possible. SwapDeltas should never be called without then calling
  // ConsumeLastDelta--otherwise the delta will be lost.
  void SwapDeltas();
  void ConsumeLastDelta();

  // The BucketBoundaries of each registered view with Distribution aggregation,
  // by measure. Array indices in the outer array correspond to measure indices.
  std::vector<std::vector<BucketBoundaries>> registered_boundaries_;
  Delta active_delta_;

  // TODO: consider making this a lockless queue to avoid blocking the main
  // thread when calling a flush during harvesting.
  Delta last_delta_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_INTERNAL_DELTA_PRODUCER_H_
