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

#ifndef OPENCENSUS_STATS_STATS_H_
#define OPENCENSUS_STATS_STATS_H_

// Re-export the public headers for stats so that users do not need to maintain
// a long include list.
#include "opencensus/stats/aggregation.h"         // IWYU pragma: export
#include "opencensus/stats/bucket_boundaries.h"   // IWYU pragma: export
#include "opencensus/stats/measure.h"             // IWYU pragma: export
#include "opencensus/stats/measure_descriptor.h"  // IWYU pragma: export
#include "opencensus/stats/measure_registry.h"    // IWYU pragma: export
#include "opencensus/stats/recording.h"           // IWYU pragma: export
#include "opencensus/stats/stats_exporter.h"      // IWYU pragma: export
#include "opencensus/stats/tag_key.h"             // IWYU pragma: export
#include "opencensus/stats/tag_set.h"             // IWYU pragma: export
#include "opencensus/stats/view.h"                // IWYU pragma: export
#include "opencensus/stats/view_data.h"           // IWYU pragma: export
#include "opencensus/stats/view_descriptor.h"     // IWYU pragma: export

#endif  // OPENCENSUS_STATS_STATS_H_
