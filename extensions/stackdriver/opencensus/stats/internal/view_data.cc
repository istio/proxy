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

#include "opencensus/stats/view_data.h"

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "opencensus/stats/internal/view_data_impl.h"

namespace opencensus {
namespace stats {

const Aggregation& ViewData::aggregation() const {
  return impl_->aggregation();
}

ViewData::Type ViewData::type() const {
  switch (impl_->type()) {
    case ViewDataImpl::Type::kDouble:
      return Type::kDouble;
    case ViewDataImpl::Type::kInt64:
      return Type::kInt64;
    case ViewDataImpl::Type::kDistribution:
      return Type::kDistribution;
    case ViewDataImpl::Type::kStatsObject:
      // This DCHECKs in the constructor. Returning kDouble here is
      // safe, albeit incorrect--the double_data() accessor will return an empty
      // map.
      return Type::kDouble;
  }
  return Type::kDouble;
}

const ViewData::DataMap<double>& ViewData::double_data() const {
  if (impl_->type() == ViewDataImpl::Type::kDouble) {
    return impl_->double_data();
  } else {
    static DataMap<double> empty_map;
    return empty_map;
  }
}

const ViewData::DataMap<int64_t>& ViewData::int_data() const {
  if (impl_->type() == ViewDataImpl::Type::kInt64) {
    return impl_->int_data();
  } else {
    static DataMap<int64_t> empty_map;
    return empty_map;
  }
}

const ViewData::DataMap<Distribution>& ViewData::distribution_data() const {
  if (impl_->type() == ViewDataImpl::Type::kDistribution) {
    return impl_->distribution_data();
  } else {
    static DataMap<Distribution> empty_map;
    return empty_map;
  }
}

uint64_t ViewData::start_time() const { return impl_->start_time(); }
uint64_t ViewData::end_time() const { return impl_->end_time(); }

ViewData::ViewData(const ViewData& other)
    : impl_(absl::make_unique<ViewDataImpl>(*other.impl_)) {}

ViewData::ViewData(std::unique_ptr<ViewDataImpl> data)
    : impl_(std::move(data)) {}

}  // namespace stats
}  // namespace opencensus
