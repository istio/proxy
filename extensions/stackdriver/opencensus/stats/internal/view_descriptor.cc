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

#include "opencensus/stats/view_descriptor.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/internal/aggregation_window.h"
#include "opencensus/stats/internal/measure_registry_impl.h"
#include "opencensus/stats/internal/stats_exporter_impl.h"
#include "opencensus/stats/measure_descriptor.h"
#include "opencensus/stats/view.h"
#include "opencensus/tags/tag_key.h"

namespace opencensus {
namespace stats {

// TODO: NICETH: Allow inserting views without an id (autogenerating one
// based on measure/aggregation/columns).
// TODO: FIXME: Distinguish never-set values, and add an IsValid()
// method checking required fields.

ViewDescriptor::ViewDescriptor()
    : aggregation_(Aggregation::Sum()),
      aggregation_window_(AggregationWindow::Cumulative()) {}

ViewDescriptor& ViewDescriptor::set_name(absl::string_view name) {
  name_ = std::string(name);
  return *this;
}

ViewDescriptor& ViewDescriptor::set_measure(absl::string_view name) {
  measure_name_ = std::string(name);
  measure_id_ = MeasureRegistryImpl::Get()->GetIdByName(name);
  return *this;
}

const MeasureDescriptor& ViewDescriptor::measure_descriptor() const {
  return MeasureRegistryImpl::Get()->GetDescriptorByName(measure_name_);
}

ViewDescriptor& ViewDescriptor::set_aggregation(
    const Aggregation& aggregation) {
  aggregation_ = aggregation;
  return *this;
}

ViewDescriptor& ViewDescriptor::add_column(opencensus::tags::TagKey tag_key) {
  columns_.emplace_back(tag_key);
  return *this;
}

ViewDescriptor& ViewDescriptor::set_description(absl::string_view description) {
  description_ = std::string(description);
  return *this;
}

void ViewDescriptor::RegisterForExport() const {
  if (aggregation_window_.type() == AggregationWindow::Type::kCumulative) {
    StatsExporterImpl::Get()->AddView(*this);
  }
}

std::string ViewDescriptor::DebugString() const {
  return absl::StrCat(
      "\n  name: \"", name_,
      "\"\n  measure: ", measure_descriptor().DebugString(),
      "\n  aggregation: ", aggregation_.DebugString(),
      "\n  aggregation window: ", aggregation_window_.DebugString(),
      "\n  columns: ",
      absl::StrJoin(columns_, ":",
                    [](std::string* out, opencensus::tags::TagKey key) {
                      return out->append(key.name());
                    }),
      "\n  description: \"", description_, "\"");
}

bool ViewDescriptor::operator==(const ViewDescriptor& other) const {
  return name_ == other.name_ && measure_id_ == other.measure_id_ &&
         aggregation_ == other.aggregation_ &&
         aggregation_window_ == other.aggregation_window_ &&
         columns_ == other.columns_ && description_ == other.description_;
}

}  // namespace stats
}  // namespace opencensus
