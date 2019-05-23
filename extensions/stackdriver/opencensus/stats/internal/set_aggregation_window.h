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

#ifndef OPENCENSUS_STATS_INTERNAL_SET_AGGREGATION_WINDOW_H_
#define OPENCENSUS_STATS_INTERNAL_SET_AGGREGATION_WINDOW_H_

#include "opencensus/stats/internal/aggregation_window.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

// You probably do not need this: ViewDescriptor has a Cumulative aggregation
// window by default, and that is what most exporters expect. Interval
// aggregation is mainly useful for on-task purposes, such as server status
// displays.
void SetAggregationWindow(const AggregationWindow& window,
                          ViewDescriptor* descriptor);

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_INTERNAL_SET_AGGREGATION_WINDOW_H_
