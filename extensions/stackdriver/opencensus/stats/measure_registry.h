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

#ifndef OPENCENSUS_STATS_MEASURE_REGISTRY_H_
#define OPENCENSUS_STATS_MEASURE_REGISTRY_H_

#include "absl/strings/string_view.h"
#include "opencensus/stats/measure.h"
#include "opencensus/stats/measure_descriptor.h"

namespace opencensus {
namespace stats {

// The MeasureRegistry keeps a record of all MeasureDescriptors registered,
// providing functions for querying their metadata by name or handle. Use
// Measure<MeasureT>::Register() to register a measure with the registry.
// MeasureRegistry is thread-safe.
class MeasureRegistry final {
 public:
  // Returns the descriptor of the measure registered under 'name' if one is
  // registered, and a descriptor with an empty name otherwise.
  static const MeasureDescriptor& GetDescriptorByName(absl::string_view name);

  // Returns a measure for the registered MeasureDescriptor with the
  // provided name, if one exists, and an invalid Measure otherwise.
  static MeasureDouble GetMeasureDoubleByName(absl::string_view name);
  static MeasureInt64 GetMeasureInt64ByName(absl::string_view name);
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_MEASURE_REGISTRY_H_
