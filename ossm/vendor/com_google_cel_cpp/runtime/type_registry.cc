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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "common/value.h"
#include "runtime/internal/legacy_runtime_type_provider.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

TypeRegistry::TypeRegistry(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nullable message_factory)
    : type_provider_(descriptor_pool),
      legacy_type_provider_(
          std::make_shared<runtime_internal::LegacyRuntimeTypeProvider>(
              descriptor_pool, message_factory)) {
  RegisterEnum("google.protobuf.NullValue", {{"NULL_VALUE", 0}});
}

void TypeRegistry::RegisterEnum(absl::string_view enum_name,
                                std::vector<Enumerator> enumerators) {
  {
    absl::MutexLock lock(&enum_value_table_mutex_);
    enum_value_table_.reset();
  }
  enum_types_[enum_name] =
      Enumeration{std::string(enum_name), std::move(enumerators)};
}

std::shared_ptr<const absl::flat_hash_map<std::string, Value>>
TypeRegistry::GetEnumValueTable() const {
  {
    absl::ReaderMutexLock lock(&enum_value_table_mutex_);
    if (enum_value_table_ != nullptr) {
      return enum_value_table_;
    }
  }

  absl::MutexLock lock(&enum_value_table_mutex_);
  if (enum_value_table_ != nullptr) {
    return enum_value_table_;
  }
  std::shared_ptr<absl::flat_hash_map<std::string, Value>> result =
      std::make_shared<absl::flat_hash_map<std::string, Value>>();

  auto& enum_value_map = *result;
  for (auto iter = enum_types_.begin(); iter != enum_types_.end(); ++iter) {
    absl::string_view enum_name = iter->first;
    const auto& enum_type = iter->second;
    for (const auto& enumerator : enum_type.enumerators) {
      auto key = absl::StrCat(enum_name, ".", enumerator.name);
      enum_value_map[key] = cel::IntValue(enumerator.number);
    }
  }

  enum_value_table_ = result;

  return result;
}
}  // namespace cel
