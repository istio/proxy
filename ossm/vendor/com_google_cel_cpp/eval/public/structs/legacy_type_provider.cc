// Copyright 2024 Google LLC
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

#include "eval/public/structs/legacy_type_provider.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/legacy_value.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::runtime::LegacyTypeAdapter;
using google::api::expr::runtime::MessageWrapper;

class LegacyStructValueBuilder final : public cel::StructValueBuilder {
 public:
  LegacyStructValueBuilder(cel::MemoryManagerRef memory_manager,
                           LegacyTypeAdapter adapter,
                           MessageWrapper::Builder builder)
      : memory_manager_(memory_manager),
        adapter_(adapter),
        builder_(std::move(builder)) {}

  absl::StatusOr<absl::optional<cel::ErrorValue>> SetFieldByName(
      absl::string_view name, cel::Value value) override {
    CEL_ASSIGN_OR_RETURN(
        auto legacy_value,
        LegacyValue(cel::extensions::ProtoMemoryManagerArena(memory_manager_),
                    value),
        _.With(cel::ErrorValueReturn()));
    CEL_RETURN_IF_ERROR(adapter_.mutation_apis()->SetField(
                            name, legacy_value, memory_manager_, builder_))
        .With(cel::ErrorValueReturn());
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<cel::ErrorValue>> SetFieldByNumber(
      int64_t number, cel::Value value) override {
    CEL_ASSIGN_OR_RETURN(
        auto legacy_value,
        LegacyValue(cel::extensions::ProtoMemoryManagerArena(memory_manager_),
                    value),
        _.With(cel::ErrorValueReturn()));
    CEL_RETURN_IF_ERROR(adapter_.mutation_apis()->SetFieldByNumber(
                            number, legacy_value, memory_manager_, builder_))
        .With(cel::ErrorValueReturn());
    return absl::nullopt;
  }

  absl::StatusOr<cel::StructValue> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto message,
                         adapter_.mutation_apis()->AdaptFromWellKnownType(
                             memory_manager_, std::move(builder_)));
    if (!message.IsMessage()) {
      return absl::FailedPreconditionError("expected MessageWrapper");
    }
    auto message_wrapper = message.MessageWrapperOrDie();
    return cel::common_internal::LegacyStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message_wrapper.message_ptr()),
        message_wrapper.legacy_type_info());
  }

 private:
  cel::MemoryManagerRef memory_manager_;
  LegacyTypeAdapter adapter_;
  MessageWrapper::Builder builder_;
};

class LegacyValueBuilder final : public cel::ValueBuilder {
 public:
  LegacyValueBuilder(cel::MemoryManagerRef memory_manager,
                     LegacyTypeAdapter adapter, MessageWrapper::Builder builder)
      : memory_manager_(memory_manager),
        adapter_(adapter),
        builder_(std::move(builder)) {}

  absl::StatusOr<absl::optional<cel::ErrorValue>> SetFieldByName(
      absl::string_view name, cel::Value value) override {
    CEL_ASSIGN_OR_RETURN(
        auto legacy_value,
        LegacyValue(cel::extensions::ProtoMemoryManagerArena(memory_manager_),
                    value),
        _.With(cel::ErrorValueReturn()));
    CEL_RETURN_IF_ERROR(adapter_.mutation_apis()->SetField(
                            name, legacy_value, memory_manager_, builder_))
        .With(cel::ErrorValueReturn());
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<cel::ErrorValue>> SetFieldByNumber(
      int64_t number, cel::Value value) override {
    CEL_ASSIGN_OR_RETURN(
        auto legacy_value,
        LegacyValue(cel::extensions::ProtoMemoryManagerArena(memory_manager_),
                    value),
        _.With(cel::ErrorValueReturn()));
    CEL_RETURN_IF_ERROR(adapter_.mutation_apis()->SetFieldByNumber(
                            number, legacy_value, memory_manager_, builder_))
        .With(cel::ErrorValueReturn());
    return absl::nullopt;
  }

  absl::StatusOr<cel::Value> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto value,
                         adapter_.mutation_apis()->AdaptFromWellKnownType(
                             memory_manager_, std::move(builder_)),
                         _.With(cel::ErrorValueReturn()));
    CEL_ASSIGN_OR_RETURN(
        auto result,
        cel::ModernValue(
            cel::extensions::ProtoMemoryManagerArena(memory_manager_), value),
        _.With(cel::ErrorValueReturn()));
    return result;
  }

 private:
  cel::MemoryManagerRef memory_manager_;
  LegacyTypeAdapter adapter_;
  MessageWrapper::Builder builder_;
};

}  // namespace

absl::StatusOr<absl_nullable cel::ValueBuilderPtr>
LegacyTypeProvider::NewValueBuilder(
    absl::string_view name,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  if (auto type_adapter = ProvideLegacyType(name); type_adapter.has_value()) {
    const auto* mutation_apis = type_adapter->mutation_apis();
    if (mutation_apis == nullptr) {
      return absl::FailedPreconditionError(
          absl::StrCat("LegacyTypeMutationApis missing for type: ", name));
    }
    CEL_ASSIGN_OR_RETURN(
        auto builder,
        mutation_apis->NewInstance(cel::MemoryManagerRef::Pooling(arena)));
    return std::make_unique<LegacyValueBuilder>(
        cel::MemoryManagerRef::Pooling(arena), *type_adapter,
        std::move(builder));
  }
  return nullptr;
}

absl::StatusOr<absl::optional<cel::Type>> LegacyTypeProvider::FindTypeImpl(
    absl::string_view name) const {
  if (auto type_info = ProvideLegacyTypeInfo(name); type_info.has_value()) {
    const auto* descriptor = (*type_info)->GetDescriptor(MessageWrapper());
    if (descriptor != nullptr) {
      return cel::MessageType(descriptor);
    }
    return cel::common_internal::MakeBasicStructType(
        (*type_info)->GetTypename(MessageWrapper()));
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<cel::StructTypeField>>
LegacyTypeProvider::FindStructTypeFieldByNameImpl(
    absl::string_view type, absl::string_view name) const {
  if (auto type_info = ProvideLegacyTypeInfo(type); type_info.has_value()) {
    if (auto field_desc = (*type_info)->FindFieldByName(name);
        field_desc.has_value()) {
      return cel::common_internal::BasicStructTypeField(
          field_desc->name, field_desc->number, cel::DynType{});
    } else {
      const auto* mutation_apis =
          (*type_info)->GetMutationApis(MessageWrapper());
      if (mutation_apis == nullptr || !mutation_apis->DefinesField(name)) {
        return absl::nullopt;
      }
      return cel::common_internal::BasicStructTypeField(name, 0,
                                                        cel::DynType{});
    }
  }
  return absl::nullopt;
}

}  // namespace google::api::expr::runtime
