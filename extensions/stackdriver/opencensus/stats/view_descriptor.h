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

#ifndef OPENCENSUS_STATS_VIEW_DESCRIPTOR_H_
#define OPENCENSUS_STATS_VIEW_DESCRIPTOR_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/internal/aggregation_window.h"
#include "opencensus/stats/measure_descriptor.h"
#include "opencensus/tags/tag_key.h"

namespace opencensus {
namespace stats {

// ViewDescriptor provides metadata for a view: a unique name, the measure to
// collect data for, how to aggregate that data, and what tag keys to break it
// down by.
// In order to collect data for a ViewDescriptor, it must either be registered
// for export (by calling RegisterForExport() on the fully-defined descriptor)
// or converted into a View to collect data on-task (see view.h).
//
// ViewDescriptor is a value type, and is thread-compatible.
class ViewDescriptor final {
 public:
  //////////////////////////////////////////////////////////////////////////////
  // View definition

  // Creates a ViewDescriptor with Cumulative aggregation.
  ViewDescriptor();

  // Sets the name of the ViewDescriptor. Names must be unique within the
  // library; it is recommended that it be in the format "<domain>/<path>",
  // where "<path>" uniquely specifies the measure, aggregation, and columns
  // (e.g. "example.com/Foo/FooUsage-sum-key1-key2").
  ViewDescriptor& set_name(absl::string_view name);
  const std::string& name() const { return name_; }

  // Sets the measure. If no measure is registered under 'name' any View created
  // with the descriptor will be invalid.
  ViewDescriptor& set_measure(absl::string_view name);
  // Accesses the descriptor of the view's measure. If no measure has been
  // registered under the name set using set_measure(), this returns an invalid
  // descriptor with blank fields.
  const MeasureDescriptor& measure_descriptor() const;

  // Sets and retrieves the ViewDescriptor's aggregation. See aggregation.h for
  // details of the options.
  ViewDescriptor& set_aggregation(const Aggregation& aggregation);
  const Aggregation& aggregation() const { return aggregation_; }

  // Adds a dimension to the view's data. When data is recorded it can specify a
  // number of tags, key-value pairs; the aggregated data for each view will be
  // broken down by the distinct values of each tag key matching one of the
  // view's columns.
  ViewDescriptor& add_column(opencensus::tags::TagKey tag_key);
  size_t num_columns() const { return columns_.size(); }
  const std::vector<opencensus::tags::TagKey>& columns() const {
    return columns_;
  }

  // Sets a human-readable description for the view.
  ViewDescriptor& set_description(absl::string_view description);
  const std::string& description() const { return description_; }

  //////////////////////////////////////////////////////////////////////////////
  // View registration

  // Registers this ViewDescriptor for export, replacing any already registered
  // view with the same name.; requires that aggregation_window() ==
  // AggregationWindow::kCumulative() (the default). Future changes to this
  // ViewDescriptor will not update the registered view.
  void RegisterForExport() const;

  //////////////////////////////////////////////////////////////////////////////
  // Utilities

  std::string DebugString() const;

  bool operator==(const ViewDescriptor& other) const;
  bool operator!=(const ViewDescriptor& other) const {
    return !(*this == other);
  }

 private:
  friend class StatsManager;
  friend class ViewDataImpl;
  friend void SetAggregationWindow(const AggregationWindow&, ViewDescriptor*);

  std::string name_;
  std::string measure_name_;
  uint64_t measure_id_;
  Aggregation aggregation_;
  AggregationWindow aggregation_window_;
  std::vector<opencensus::tags::TagKey> columns_;
  std::string description_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_VIEW_DESCRIPTOR_H_
