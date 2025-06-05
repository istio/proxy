// Copyright 2021 Google LLC
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

#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/type_provider.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value.h"
#include "eval/internal/interop.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "google/protobuf/descriptor.h"

namespace google::api::expr::runtime {

namespace {

using cel::Type;
using cel::TypeFactory;

class LegacyToModernTypeProviderAdapter : public LegacyTypeProvider {
 public:
  explicit LegacyToModernTypeProviderAdapter(const LegacyTypeProvider& provider)
      : provider_(provider) {}

  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    return provider_.ProvideLegacyType(name);
  }

  absl::optional<const LegacyTypeInfoApis*> ProvideLegacyTypeInfo(
      absl::string_view name) const override {
    return provider_.ProvideLegacyTypeInfo(name);
  }

 private:
  const LegacyTypeProvider& provider_;
};

void AddEnumFromDescriptor(const google::protobuf::EnumDescriptor* desc,
                           CelTypeRegistry& registry) {
  std::vector<CelTypeRegistry::Enumerator> enumerators;
  enumerators.reserve(desc->value_count());
  for (int i = 0; i < desc->value_count(); i++) {
    enumerators.push_back(
        {std::string(desc->value(i)->name()), desc->value(i)->number()});
  }
  registry.RegisterEnum(desc->full_name(), std::move(enumerators));
}

}  // namespace

CelTypeRegistry::CelTypeRegistry() = default;

void CelTypeRegistry::Register(const google::protobuf::EnumDescriptor* enum_descriptor) {
  AddEnumFromDescriptor(enum_descriptor, *this);
}

void CelTypeRegistry::RegisterEnum(absl::string_view enum_name,
                                   std::vector<Enumerator> enumerators) {
  modern_type_registry_.RegisterEnum(enum_name, std::move(enumerators));
}

void CelTypeRegistry::RegisterTypeProvider(
    std::unique_ptr<LegacyTypeProvider> provider) {
  legacy_type_providers_.push_back(
      std::shared_ptr<const LegacyTypeProvider>(std::move(provider)));
  modern_type_registry_.AddTypeProvider(
      std::make_unique<LegacyToModernTypeProviderAdapter>(
          *legacy_type_providers_.back()));
}

std::shared_ptr<const LegacyTypeProvider>
CelTypeRegistry::GetFirstTypeProvider() const {
  if (legacy_type_providers_.empty()) {
    return nullptr;
  }
  return legacy_type_providers_[0];
}

// Find a type's CelValue instance by its fully qualified name.
absl::optional<LegacyTypeAdapter> CelTypeRegistry::FindTypeAdapter(
    absl::string_view fully_qualified_type_name) const {
  for (const auto& provider : legacy_type_providers_) {
    auto maybe_adapter = provider->ProvideLegacyType(fully_qualified_type_name);
    if (maybe_adapter.has_value()) {
      return maybe_adapter;
    }
  }

  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
