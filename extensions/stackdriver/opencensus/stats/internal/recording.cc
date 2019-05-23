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

#include "opencensus/stats/recording.h"

#include <initializer_list>

#include "opencensus/stats/internal/delta_producer.h"
#include "opencensus/stats/measure.h"
#include "opencensus/tags/context_util.h"
#include "opencensus/tags/tag_map.h"

namespace opencensus {
namespace stats {

void Record(std::initializer_list<Measurement> measurements) {
  DeltaProducer::Get()->Record(measurements,
                               opencensus::tags::GetCurrentTagMap());
}

void Record(std::initializer_list<Measurement> measurements,
            opencensus::tags::TagMap tags) {
  DeltaProducer::Get()->Record(measurements, std::move(tags));
}

bool Flush() { return DeltaProducer::Get()->Flush(); }

}  // namespace stats
}  // namespace opencensus
