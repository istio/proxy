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

#ifndef OPENCENSUS_STATS_VIEW_H_
#define OPENCENSUS_STATS_VIEW_H_

#include "opencensus/stats/internal/stats_manager.h"
#include "opencensus/stats/view_data.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

// View is an RAII handle for on-task data collection--once a View is
// instantiated, OpenCensus will collect data for it, which can be accessed with
// View::GetData(). To register a view for export, rather than on-task
// collection, use ViewDescriptor::RegisterForExport() instead.
//
// View objects are thread-safe.
class View {
 public:
  // Creates a view, starting data collection for it. If descriptor.measure()
  // has not been registered, IsValid() on the returned object will return
  // false and GetData() will return an empty ViewData.
  View(const ViewDescriptor& descriptor);

  // Not copyable, since views are RAII handles for resource collection.
  View(const View& view) = delete;
  View& operator=(const View& view) = delete;

  // Stops data collection.
  ~View();

  // Returns true if this object is valid and data can be collected.
  bool IsValid() const;

  // Returns a snapshot of the View's data.
  const ViewData GetData();

  // TODO: Consider a means of querying one tagset to avoid copying.

  const ViewDescriptor& descriptor() { return descriptor_; }

 private:
  const ViewDescriptor descriptor_;
  StatsManager::ViewInformation* const handle_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_VIEW_H_
