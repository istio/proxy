// Copyright 2023 Google LLC
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

#include "google/protobuf/util/converter/type_info.h"

#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/type.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/utility.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

namespace {

template <typename T>
static void DeleteCachedTypes(
    absl::flat_hash_map<std::string, absl::StatusOr<T*>>* cached_types) {
  for (auto& [key, value] : *cached_types) {
    if (value.ok()) {
      delete value.value();
    }
  }
}

// A TypeInfo that looks up information provided by a TypeResolver.
class TypeInfoForTypeResolver : public TypeInfo {
 public:
  explicit TypeInfoForTypeResolver(TypeResolver* type_resolver)
      : type_resolver_(type_resolver) {}

  ~TypeInfoForTypeResolver() override {
    DeleteCachedTypes(&cached_types_);
    DeleteCachedTypes(&cached_enums_);
  }

  absl::StatusOr<const google::protobuf::Type*> ResolveTypeUrl(
      absl::string_view type_url) const override {
    auto it = cached_types_.find(type_url);
    if (it != cached_types_.end()) {
      return it->second;
    }
    std::string string_type_url(type_url);
    std::unique_ptr<google::protobuf::Type> type(new google::protobuf::Type());
    absl::Status status =
        type_resolver_->ResolveMessageType(string_type_url, type.get());
    StatusOrType result =
        status.ok() ? StatusOrType(type.release()) : StatusOrType(status);
    it = cached_types_.emplace(std::move(string_type_url), std::move(result))
             .first;
    return it->second;
  }

  const google::protobuf::Type* GetTypeByTypeUrl(
      absl::string_view type_url) const override {
    StatusOrType result = ResolveTypeUrl(type_url);
    return result.ok() ? result.value() : NULL;
  }

  const google::protobuf::Enum* GetEnumByTypeUrl(
      absl::string_view type_url) const override {
    auto it = cached_enums_.find(type_url);
    if (it != cached_enums_.end()) {
      return it->second.value_or(nullptr);
    }
    std::string string_type_url(type_url);
    std::unique_ptr<google::protobuf::Enum> enum_type(
        new google::protobuf::Enum());
    absl::Status status =
        type_resolver_->ResolveEnumType(string_type_url, enum_type.get());
    StatusOrEnum result =
        status.ok() ? StatusOrEnum(enum_type.release()) : StatusOrEnum(status);
    it = cached_enums_.emplace(std::move(string_type_url), std::move(result))
             .first;
    return it->second.value_or(nullptr);
  }

  const google::protobuf::Field* FindField(
      const google::protobuf::Type* type,
      absl::string_view camel_case_name) const override {
    auto it = indexed_types_.find(type);
    const CamelCaseNameTable& camel_case_name_table =
        it == indexed_types_.end()
            ? PopulateNameLookupTable(type, &indexed_types_[type])
            : it->second;

    absl::string_view name = camel_case_name;
    auto cc_it = camel_case_name_table.find(name);
    if (cc_it != camel_case_name_table.end()) {
      name = cc_it->second;
    }
    return FindFieldInTypeOrNull(type, name);
  }

 private:
  typedef absl::StatusOr<const google::protobuf::Type*> StatusOrType;
  typedef absl::StatusOr<const google::protobuf::Enum*> StatusOrEnum;
  typedef absl::flat_hash_map<absl::string_view, absl::string_view>
      CamelCaseNameTable;

  const CamelCaseNameTable& PopulateNameLookupTable(
      const google::protobuf::Type* type,
      CamelCaseNameTable* camel_case_name_table) const {
    for (int i = 0; i < type->fields_size(); ++i) {
      const google::protobuf::Field& field = type->fields(i);
      absl::string_view existing =
          camel_case_name_table->insert({field.json_name(), field.name()})
              .first->second;
      if (existing != field.name()) {
        ABSL_LOG(WARNING) << "Field '" << field.name() << "' and '" << existing
                          << "' map to the same camel case name '"
                          << field.json_name() << "'.";
      }
    }
    return *camel_case_name_table;
  }

  TypeResolver* type_resolver_;

  mutable absl::flat_hash_map<std::string, StatusOrType> cached_types_;
  mutable absl::flat_hash_map<std::string, StatusOrEnum> cached_enums_;

  mutable absl::flat_hash_map<const google::protobuf::Type*, CamelCaseNameTable>
      indexed_types_;
};
}  // namespace

TypeInfo* TypeInfo::NewTypeInfo(TypeResolver* type_resolver) {
  return new TypeInfoForTypeResolver(type_resolver);
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
