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

#ifndef OPENCENSUS_STATS_INTERNAL_VIEW_DATA_IMPL_H_
#define OPENCENSUS_STATS_INTERNAL_VIEW_DATA_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "opencensus/common/internal/string_vector_hash.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/distribution.h"
#include "opencensus/stats/internal/aggregation_window.h"
#include "opencensus/stats/internal/measure_data.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

// ViewDataImpl contains a snapshot of data for a particular View. DataValueT is
// the type of the returned data, with possibilities listed in
// data_value_type.h. Which value type is returned for a view is determined by
// the view's aggregation and aggregation window.
//
// Thread-compatible.
class ViewDataImpl {
 public:
  // A convenience alias for the type of the map from tags to data.
  template <typename DataValueT>
  using DataMap = std::unordered_map<std::vector<std::string>, DataValueT,
                                     common::StringVectorHash>;

  // Constructs an empty ViewDataImpl for internal use from the descriptor. A
  // ViewData can be constructed directly from such a ViewDataImpl for
  // snapshotting cumulative data; ViewDataImpls for interval views must be
  // converted using the following constructor before snapshotting.
  ViewDataImpl(uint64_t start_time, const ViewDescriptor& descriptor);

  ViewDataImpl(const ViewDataImpl& other);
  ~ViewDataImpl();

  // Returns a copy of the present state of the object and resets data() and
  // start_time().
  std::unique_ptr<ViewDataImpl> GetDeltaAndReset(uint64_t now);

  const Aggregation& aggregation() const { return aggregation_; }
  const AggregationWindow& aggregation_window() const {
    return aggregation_window_;
  }

  enum class Type {
    kDouble,
    kInt64,
    kDistribution,
    kStatsObject,  // Used for aggregating data, should not be exported.
  };
  Type type() const { return type_; }

  // A map from tag values (corresponding to the keys in the ViewDescriptor, in
  // that order) to the data for those tags. What data is contained depends on
  // the View's Aggregation and AggregationWindow.
  // Only one of these is valid for any ViewDataImpl (which is indicated by
  // type());
  const DataMap<double>& double_data() const { return double_data_; }
  const DataMap<int64_t>& int_data() const { return int_data_; }
  const DataMap<Distribution>& distribution_data() const {
    return distribution_data_;
  }

  uint64_t start_time() const { return start_time_; }
  uint64_t end_time() const { return end_time_; }

  // Merges bulk data for the given tag values at 'now'. tag_values must be
  // ordered according to the order of keys in the ViewDescriptor.
  // TODO: Change to take Span<string_view> when heterogenous lookup is
  // supported.
  void Merge(const std::vector<std::string>& tag_values,
             const MeasureData& data, uint64_t now);

 private:
  // Implements GetDeltaAndReset(), copying aggregation_ and swapping data_ and
  // start/end times. This is private so that it can be given a more descriptive
  // name in the public API.
  ViewDataImpl(ViewDataImpl* source, uint64_t now);

  Type TypeForDescriptor(const ViewDescriptor& descriptor);

  const Aggregation aggregation_;
  const AggregationWindow aggregation_window_;
  const Type type_;
  union {
    DataMap<double> double_data_;
    DataMap<int64_t> int_data_;
    DataMap<Distribution> distribution_data_;
  };

  uint64_t start_time_;
  uint64_t end_time_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_INTERNAL_VIEW_DATA_IMPL_H_
