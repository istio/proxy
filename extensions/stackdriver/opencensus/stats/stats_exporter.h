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

#ifndef OPENCENSUS_STATS_STATS_EXPORTER_H_
#define OPENCENSUS_STATS_STATS_EXPORTER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "opencensus/stats/view.h"
#include "opencensus/stats/view_data.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

// StatsExporter manages views for export, and export handlers. New views can be
// registered with ViewDescriptor::RegisterForExport().
// StatsExporter is thread-safe.
class StatsExporter final {
 public:
  // Removes the view with 'name' from the registry, if one is registered.
  static void RemoveView(absl::string_view name);

  // StatsExporter::Handler is the interface for push exporters that export
  // recorded data for registered views. The exporter should provide a static
  // Register() method that takes any arguments needed by the exporter (e.g. a
  // URL to export to) and calls StatsExporter::RegisterHandler itself.
  class Handler {
   public:
    virtual ~Handler() = default;
    virtual void ExportViewData(
        const std::vector<std::pair<ViewDescriptor, ViewData>>& data) = 0;
  };

  // Registers a new handler. Every few seconds, each registered handler will be
  // called with the present data for each registered view. This should only be
  // called by push exporters' Register() methods.
  static void RegisterPushHandler(std::unique_ptr<Handler> handler);

  // Retrieves current data for all registered views, for implementing pull
  // exporters.
  static std::vector<std::pair<ViewDescriptor, ViewData>> GetViewData();

  // Exports view data to backend handlers.
  static void ExportViewData();
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_STATS_EXPORTER_H_
