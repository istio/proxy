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

#include "opencensus/stats/internal/view_data_impl.h"

#include <cstdint>
#include <memory>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "opencensus/stats/distribution.h"
#include "opencensus/stats/measure_descriptor.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

ViewDataImpl::Type ViewDataImpl::TypeForDescriptor(
    const ViewDescriptor& descriptor) {
  switch (descriptor.aggregation_window_.type()) {
    case AggregationWindow::Type::kCumulative:
    case AggregationWindow::Type::kDelta:
      switch (descriptor.aggregation().type()) {
        case Aggregation::Type::kSum:
        case Aggregation::Type::kLastValue:
          switch (descriptor.measure_descriptor().type()) {
            case MeasureDescriptor::Type::kDouble:
              return ViewDataImpl::Type::kDouble;
            case MeasureDescriptor::Type::kInt64:
              return ViewDataImpl::Type::kInt64;
          }
        case Aggregation::Type::kCount:
          return ViewDataImpl::Type::kInt64;
        case Aggregation::Type::kDistribution:
          return ViewDataImpl::Type::kDistribution;
      }
    case AggregationWindow::Type::kInterval:
      return ViewDataImpl::Type::kStatsObject;
  }
  return ViewDataImpl::Type::kDouble;
}

ViewDataImpl::ViewDataImpl(uint64_t start_time,
                           const ViewDescriptor& descriptor)
    : aggregation_(descriptor.aggregation()),
      aggregation_window_(descriptor.aggregation_window_),
      type_(TypeForDescriptor(descriptor)),
      start_time_(start_time),
      end_time_(start_time + 1) {
  switch (type_) {
    case Type::kDouble: {
      new (&double_data_) DataMap<double>();
      break;
    }
    case Type::kInt64: {
      new (&int_data_) DataMap<int64_t>();
      break;
    }
    case Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>();
      break;
    }
    case Type::kStatsObject: {
      break;
    }
  }
}

ViewDataImpl::~ViewDataImpl() {
  switch (type_) {
    case Type::kDouble: {
      double_data_.~DataMap<double>();
      break;
    }
    case Type::kInt64: {
      int_data_.~DataMap<int64_t>();
      break;
    }
    case Type::kDistribution: {
      distribution_data_.~DataMap<Distribution>();
      break;
    }
    case Type::kStatsObject: {
      break;
    }
  }
}

std::unique_ptr<ViewDataImpl> ViewDataImpl::GetDeltaAndReset(uint64_t now) {
  // Need to use wrap_unique because this is a private constructor.
  return absl::WrapUnique(new ViewDataImpl(this, now));
}

ViewDataImpl::ViewDataImpl(const ViewDataImpl& other)
    : aggregation_(other.aggregation_),
      aggregation_window_(other.aggregation_window_),
      type_(other.type()),
      start_time_(other.start_time_),
      end_time_(other.end_time_) {
  switch (type_) {
    case Type::kDouble: {
      new (&double_data_) DataMap<double>(other.double_data_);
      break;
    }
    case Type::kInt64: {
      new (&int_data_) DataMap<int64_t>(other.int_data_);
      break;
    }
    case Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>(other.distribution_data_);
      break;
    }
    case Type::kStatsObject: {
      break;
    }
  }
}

void ViewDataImpl::Merge(const std::vector<std::string>& tag_values,
                         const MeasureData& data, uint64_t now) {
  end_time_ = std::max(end_time_, now);
  switch (type_) {
    case Type::kDouble: {
      if (aggregation_.type() == Aggregation::Type::kSum) {
        double_data_[tag_values] += data.sum();
      } else {
        double_data_[tag_values] = data.last_value();
      }
      break;
    }
    case Type::kInt64: {
      switch (aggregation_.type()) {
        case Aggregation::Type::kCount: {
          int_data_[tag_values] += data.count();
          break;
        }
        case Aggregation::Type::kSum: {
          int_data_[tag_values] += data.sum();
          break;
        }
        case Aggregation::Type::kLastValue: {
          int_data_[tag_values] = data.last_value();
          break;
        }
        default:
          break;
      }
      break;
    }
    case Type::kDistribution: {
      DataMap<Distribution>::iterator it = distribution_data_.find(tag_values);
      if (it == distribution_data_.end()) {
        it = distribution_data_.emplace_hint(
            it, tag_values, Distribution(&aggregation_.bucket_boundaries()));
      }
      data.AddToDistribution(&it->second);
      break;
    }
    case Type::kStatsObject: {
      break;
    }
  }
}

ViewDataImpl::ViewDataImpl(ViewDataImpl* source, uint64_t now)
    : aggregation_(source->aggregation_),
      aggregation_window_(source->aggregation_window_),
      type_(source->type_),
      start_time_(source->start_time_),
      end_time_(now) {
  switch (type_) {
    case Type::kDouble: {
      new (&double_data_) DataMap<double>();
      double_data_.swap(source->double_data_);
      break;
    }
    case Type::kInt64: {
      new (&int_data_) DataMap<int64_t>();
      int_data_.swap(source->int_data_);
      break;
    }
    case Type::kDistribution: {
      new (&distribution_data_) DataMap<Distribution>();
      distribution_data_.swap(source->distribution_data_);
      break;
    }
    case Type::kStatsObject: {
      break;
    }
  }
  source->start_time_ = now;
  source->end_time_ = now;
}

}  // namespace stats
}  // namespace opencensus
