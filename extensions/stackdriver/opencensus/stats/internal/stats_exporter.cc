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

#include "opencensus/stats/stats_exporter.h"
#include "opencensus/stats/internal/stats_exporter_impl.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "opencensus/stats/internal/aggregation_window.h"
#include "opencensus/stats/view_data.h"
#include "opencensus/stats/view_descriptor.h"

namespace opencensus {
namespace stats {

// static
StatsExporterImpl* StatsExporterImpl::Get() {
  static StatsExporterImpl* global_stats_exporter_impl =
      new StatsExporterImpl();
  return global_stats_exporter_impl;
}

void StatsExporterImpl::AddView(const ViewDescriptor& view) {
  views_[view.name()] = absl::make_unique<opencensus::stats::View>(view);
}

void StatsExporterImpl::RemoveView(absl::string_view name) {
  views_.erase(std::string(name));
}

void StatsExporterImpl::RegisterPushHandler(
    std::unique_ptr<StatsExporter::Handler> handler) {
  handlers_.push_back(std::move(handler));
}

std::vector<std::pair<ViewDescriptor, ViewData>>
StatsExporterImpl::GetViewData() {
  std::vector<std::pair<ViewDescriptor, ViewData>> data;
  data.reserve(views_.size());
  for (const auto& view : views_) {
    data.emplace_back(view.second->descriptor(), view.second->GetData());
  }
  return data;
}

void StatsExporterImpl::Export() {
  std::vector<std::pair<ViewDescriptor, ViewData>> data;
  data.reserve(views_.size());
  for (const auto& view : views_) {
    data.emplace_back(view.second->descriptor(), view.second->GetData());
  }
  for (auto& handler : handlers_) {
    handler->ExportViewData(data);
  }
}

void StatsExporterImpl::ClearHandlersForTesting() { handlers_.clear(); }

void StatsExporter::RemoveView(absl::string_view name) {
  StatsExporterImpl::Get()->RemoveView(name);
}

void StatsExporter::RegisterPushHandler(std::unique_ptr<Handler> handler) {
  StatsExporterImpl::Get()->RegisterPushHandler(std::move(handler));
}

std::vector<std::pair<ViewDescriptor, ViewData>> StatsExporter::GetViewData() {
  return StatsExporterImpl::Get()->GetViewData();
}

void StatsExporter::ExportViewData() { StatsExporterImpl::Get()->Export(); }

}  // namespace stats
}  // namespace opencensus
