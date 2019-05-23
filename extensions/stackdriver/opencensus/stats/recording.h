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

#ifndef OPENCENSUS_STATS_RECORDING_H_
#define OPENCENSUS_STATS_RECORDING_H_

#include <initializer_list>

#include "opencensus/stats/measure.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace stats {

// Records a list of Measurements under the current Context's tags. The
// recommended style is to create Measurements as an initializer list, e.g.:
//
//   Record({{measure_double, 2.5}, {measure_int, 1ll}});
//
// Only floating point values may be recorded against MeasureDoubles and only
// integral values against MeasureInt64s, to prevent silent loss of precision.
// If a record call fails to compile, ensure that all types match (using
// static_cast to double or int64_t if necessary).
void Record(std::initializer_list<Measurement> measurements);

// Records a list of Measurements under the specified 'tags'. The current
// Context's tags are ignored. e.g:
//
//   Record({{measure_double, 2.5}}, {{key, "value"}});
void Record(std::initializer_list<Measurement> measurements,
            opencensus::tags::TagMap tags);

// Flushs all recorded Measurements.
bool Flush();

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_RECORDING_H_
