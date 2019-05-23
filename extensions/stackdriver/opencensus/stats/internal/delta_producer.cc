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

#include "opencensus/stats/internal/delta_producer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "opencensus/stats/bucket_boundaries.h"
#include "opencensus/stats/internal/measure_data.h"
#include "opencensus/stats/internal/measure_registry_impl.h"
#include "opencensus/stats/internal/stats_manager.h"

namespace opencensus {
namespace stats {

void Delta::Record(std::initializer_list<Measurement> measurements,
                   opencensus::tags::TagMap tags) {
  auto it = delta_.find(tags);
  if (it == delta_.end()) {
    it = delta_.emplace_hint(it, std::piecewise_construct,
                             std::make_tuple(std::move(tags)),
                             std::make_tuple(std::vector<MeasureData>()));
    it->second.reserve(registered_boundaries_.size());
    for (const auto& boundaries_for_measure : registered_boundaries_) {
      it->second.emplace_back(boundaries_for_measure);
    }
  }
  for (const auto& measurement : measurements) {
    const uint64_t index = MeasureRegistryImpl::IdToIndex(measurement.id_);
    switch (MeasureRegistryImpl::IdToType(measurement.id_)) {
      case MeasureDescriptor::Type::kDouble:
        it->second[index].Add(measurement.value_double_);
        break;
      case MeasureDescriptor::Type::kInt64:
        it->second[index].Add(measurement.value_int_);
        break;
    }
  }
}

void Delta::clear() {
  registered_boundaries_.clear();
  delta_.clear();
}

void Delta::SwapAndReset(
    std::vector<std::vector<BucketBoundaries>>& registered_boundaries,
    Delta* other) {
  registered_boundaries_.swap(other->registered_boundaries_);
  delta_.swap(other->delta_);
  delta_.clear();
  registered_boundaries_ = registered_boundaries;
}

DeltaProducer* DeltaProducer::Get() {
  static DeltaProducer* global_delta_producer = new DeltaProducer;
  return global_delta_producer;
}

void DeltaProducer::AddMeasure() {
  registered_boundaries_.push_back({});
  SwapDeltas();
  ConsumeLastDelta();
}

void DeltaProducer::AddBoundaries(uint64_t index,
                                  const BucketBoundaries& boundaries) {
  auto& measure_boundaries = registered_boundaries_[index];
  if (std::find(measure_boundaries.begin(), measure_boundaries.end(),
                boundaries) == measure_boundaries.end()) {
    measure_boundaries.push_back(boundaries);
    SwapDeltas();
    ConsumeLastDelta();
  } else {
  }
}

void DeltaProducer::Record(std::initializer_list<Measurement> measurements,
                           opencensus::tags::TagMap tags) {
  active_delta_.Record(measurements, std::move(tags));
}

bool DeltaProducer::Flush() {
  SwapDeltas();
  if (last_delta_.delta().size() == 0) {
    return false;
  }
  ConsumeLastDelta();
  return true;
}

DeltaProducer::DeltaProducer() {}

void DeltaProducer::SwapDeltas() {
  active_delta_.SwapAndReset(registered_boundaries_, &last_delta_);
}

void DeltaProducer::ConsumeLastDelta() {
  StatsManager::Get()->MergeDelta(last_delta_);
  last_delta_.clear();
}

}  // namespace stats
}  // namespace opencensus
