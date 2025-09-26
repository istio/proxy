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
#include "extensions/protobuf/enum_adapter.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "runtime/type_registry.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

absl::Status RegisterProtobufEnum(
    TypeRegistry& registry, const google::protobuf::EnumDescriptor* enum_descriptor) {
  if (registry.resolveable_enums().contains(enum_descriptor->full_name())) {
    return absl::AlreadyExistsError(
        absl::StrCat(enum_descriptor->full_name(), " already registered."));
  }

  // TODO(uncreated-issue/42): the registry enum implementation runs linear lookups for
  // constants since this isn't expected to happen at runtime. Consider updating
  // if / when strong enum typing is implemented.
  std::vector<TypeRegistry::Enumerator> enumerators;
  enumerators.reserve(enum_descriptor->value_count());
  for (int i = 0; i < enum_descriptor->value_count(); i++) {
    enumerators.push_back({std::string(enum_descriptor->value(i)->name()),
                           enum_descriptor->value(i)->number()});
  }
  registry.RegisterEnum(enum_descriptor->full_name(), std::move(enumerators));

  return absl::OkStatus();
}

}  // namespace cel::extensions
