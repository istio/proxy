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

#ifndef OPENCENSUS_STATS_VIEW_DATA_H_
#define OPENCENSUS_STATS_VIEW_DATA_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/string_view.h"
#include "opencensus/common/internal/string_vector_hash.h"
#include "opencensus/stats/aggregation.h"
#include "opencensus/stats/distribution.h"

namespace opencensus {
namespace stats {

// Forward declarations of friends.
class ViewDataImpl;

// ViewData is an immutable snapshot of data for a particular View, aggregated
// according to the View's Aggregation and AggregationWindow.
class ViewData {
 public:
  // Maps a vector of tag values (corresponding to the columns of the
  // ViewDescriptor of the View generating this ViewData, in that order) to
  // data.
  template <typename DataValueT>
  using DataMap = std::unordered_map<std::vector<std::string>, DataValueT,
                                     common::StringVectorHash>;

  const Aggregation& aggregation() const;

  enum class Type {
    kDouble,
    kInt64,
    kDistribution,
  };
  Type type() const;

  // A map from tag values (corresponding to the keys in the ViewDescriptor, in
  // that order) to the data for those tags. What data is contained depends on
  // the View's Aggregation and AggregationWindow.
  // Only one of these is valid for any ViewData (which is valid is indicated by
  // type()). Calling the wrong one DCHECKs and returns an empty map.
  const DataMap<double>& double_data() const;
  const DataMap<int64_t>& int_data() const;
  const DataMap<Distribution>& distribution_data() const;

  uint64_t start_time() const;
  uint64_t end_time() const;

  ViewData(const ViewData& other);

 private:
  friend class View;  // Allowed to call the private constructor.
  explicit ViewData(std::unique_ptr<ViewDataImpl> data);

  const std::unique_ptr<ViewDataImpl> impl_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_VIEW_DATA_H_
