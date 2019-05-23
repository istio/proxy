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

#include "opencensus/stats/internal/stats_manager.h"

#include <memory>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/bucket_boundaries.h"
#include "opencensus/stats/internal/delta_producer.h"
#include "opencensus/stats/internal/measure_data.h"
#include "opencensus/stats/internal/measure_registry_impl.h"
#include "opencensus/stats/view_descriptor.h"
#include "opencensus/tags/tag_key.h"
#include "opencensus/tags/tag_map.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else
#include "extensions/common/wasm/null/null.h"
using namespace Envoy::Extensions::Common::Wasm::Null::Plugin;
#endif

namespace opencensus {
namespace stats {

// TODO: See if it is possible to replace AssertHeld() with function
// annotations.
// TODO: Optimize selecting/sorting tag values for each view.

// ========================================================================== //
// StatsManager::ViewInformation

StatsManager::ViewInformation::ViewInformation(const ViewDescriptor& descriptor)
    : descriptor_(descriptor),
      data_(proxy_getCurrentTimeNanoseconds(), descriptor) {}

bool StatsManager::ViewInformation::Matches(
    const ViewDescriptor& descriptor) const {
  return descriptor.aggregation() == descriptor_.aggregation() &&
         descriptor.aggregation_window_ == descriptor_.aggregation_window_ &&
         descriptor.columns() == descriptor_.columns();
}

int StatsManager::ViewInformation::num_consumers() const {
  return num_consumers_;
}

void StatsManager::ViewInformation::AddConsumer() { ++num_consumers_; }

int StatsManager::ViewInformation::RemoveConsumer() { return --num_consumers_; }

void StatsManager::ViewInformation::MergeMeasureData(
    const opencensus::tags::TagMap& tags, const MeasureData& data,
    uint64_t now) {
  std::vector<std::string> tag_values(descriptor_.columns().size());
  for (uint32_t i = 0; i < tag_values.size(); ++i) {
    const opencensus::tags::TagKey column = descriptor_.columns()[i];
    for (const auto& tag : tags.tags()) {
      if (tag.first == column) {
        tag_values[i] = std::string(tag.second);
        break;
      }
    }
  }
  data_.Merge(tag_values, data, now);
}

std::unique_ptr<ViewDataImpl> StatsManager::ViewInformation::GetData() {
  if (descriptor_.aggregation_window_.type() ==
      AggregationWindow::Type::kDelta) {
    return data_.GetDeltaAndReset(proxy_getCurrentTimeNanoseconds());
  } else {
    return absl::make_unique<ViewDataImpl>(data_);
  }
}

// ==========================================================================
// // StatsManager::MeasureInformation

void StatsManager::MeasureInformation::MergeMeasureData(
    const opencensus::tags::TagMap& tags, const MeasureData& data,
    uint64_t now) {
  for (auto& view : views_) {
    view->MergeMeasureData(tags, data, now);
  }
}

StatsManager::ViewInformation* StatsManager::MeasureInformation::AddConsumer(
    const ViewDescriptor& descriptor) {
  for (auto& view : views_) {
    if (view->Matches(descriptor)) {
      view->AddConsumer();
      return view.get();
    }
  }
  views_.emplace_back(new ViewInformation(descriptor));
  return views_.back().get();
}

void StatsManager::MeasureInformation::RemoveView(
    const ViewInformation* handle) {
  for (auto it = views_.begin(); it != views_.end(); ++it) {
    if (it->get() == handle) {
      views_.erase(it);
      return;
    }
  }
}

// ==========================================================================
// // StatsManager

// static
StatsManager* StatsManager::Get() {
  static StatsManager* global_stats_manager = new StatsManager();
  return global_stats_manager;
}

void StatsManager::MergeDelta(const Delta& delta) {
  uint64_t now = proxy_getCurrentTimeNanoseconds();
  // Measures are added to the StatsManager before the DeltaProducer, so there
  // should never be measures in the delta missing from measures_.
  for (const auto& data_for_tagset : delta.delta()) {
    for (uint32_t i = 0; i < data_for_tagset.second.size(); ++i) {
      // Only add data if there is data for this tagset/measure combination, to
      // avoid creating spurious empty rows.
      if (data_for_tagset.second[i].count() != 0) {
        measures_[i].MergeMeasureData(data_for_tagset.first,
                                      data_for_tagset.second[i], now);
      }
    }
  }
}

template <typename MeasureT>
void StatsManager::AddMeasure(Measure<MeasureT> /* measure */) {
  measures_.emplace_back(MeasureInformation());
}

template void StatsManager::AddMeasure(MeasureDouble measure);
template void StatsManager::AddMeasure(MeasureInt64 measure);

StatsManager::ViewInformation* StatsManager::AddConsumer(
    const ViewDescriptor& descriptor) {
  if (!MeasureRegistryImpl::IdValid(descriptor.measure_id_)) {
    return nullptr;
  }
  const uint64_t index = MeasureRegistryImpl::IdToIndex(descriptor.measure_id_);
  // We need to call this outside of the locked portion to avoid a deadlock when
  // the DeltaProducer flushes the old delta. We call it before adding the view
  // to avoid errors from the old delta not having a histogram for the new view.
  if (descriptor.aggregation().type() == Aggregation::Type::kDistribution) {
    DeltaProducer::Get()->AddBoundaries(
        index, descriptor.aggregation().bucket_boundaries());
  }
  return measures_[index].AddConsumer(descriptor);
}

void StatsManager::RemoveConsumer(ViewInformation* handle) {
  const int num_consumers_remaining = handle->RemoveConsumer();
  if (num_consumers_remaining == 0) {
    const auto& descriptor = handle->view_descriptor();
    const uint64_t index =
        MeasureRegistryImpl::IdToIndex(descriptor.measure_id_);
    measures_[index].RemoveView(handle);
  }
}

}  // namespace stats
}  // namespace opencensus
