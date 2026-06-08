// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/types/map_type_pool.h"

#include "common/type.h"

namespace cel::common_internal {

MapType MapTypePool::InternMapType(const Type& key, const Type& value) {
  if (key.IsDyn() && value.IsDyn()) {
    return MapType();
  }
  return *map_types_.lazy_emplace(AsTuple(key, value), [&](const auto& ctor) {
    ctor(MapType(arena_, key, value));
  });
}

}  // namespace cel::common_internal
