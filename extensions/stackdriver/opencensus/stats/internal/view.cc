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

#include "opencensus/stats/view.h"

#include <utility>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "opencensus/stats/distribution.h"
#include "opencensus/stats/internal/view_data_impl.h"

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else
#include "extensions/common/wasm/null/null.h"
using namespace Envoy::Extensions::Common::Wasm::Null::Plugin;
#endif

namespace opencensus {
namespace stats {

View::View(const ViewDescriptor& descriptor)
    : descriptor_(descriptor),
      handle_(StatsManager::Get()->AddConsumer(descriptor)) {}

View::~View() {
  if (IsValid()) {
    StatsManager::Get()->RemoveConsumer(handle_);
  }
}

bool View::IsValid() const { return handle_ != nullptr; }

const ViewData View::GetData() {
  if (!IsValid()) {
    return ViewData(absl::make_unique<ViewDataImpl>(
        proxy_getCurrentTimeNanoseconds(), descriptor_));
  }
  return ViewData(handle_->GetData());
}

}  // namespace stats
}  // namespace opencensus
