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

#include "runtime/type_registry.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace cel {

TypeRegistry::TypeRegistry() {
  RegisterEnum("google.protobuf.NullValue", {{"NULL_VALUE", 0}});
}

void TypeRegistry::RegisterEnum(absl::string_view enum_name,
                                std::vector<Enumerator> enumerators) {
  enum_types_[enum_name] =
      Enumeration{std::string(enum_name), std::move(enumerators)};
}

}  // namespace cel
