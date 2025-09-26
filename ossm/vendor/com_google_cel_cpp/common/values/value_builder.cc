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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/call_once.h"
#include "absl/base/casts.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/allocator.h"
#include "common/arena.h"
#include "common/legacy_value.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "internal/manual.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace common_internal {

namespace {

using ::cel::well_known_types::ListValueReflection;
using ::cel::well_known_types::StructReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::api::expr::runtime::CelValue;

using ValueVector = std::vector<Value, ArenaAllocator<Value>>;

absl::Status CheckListElement(const Value& value) {
  if (auto error_value = value.AsError(); ABSL_PREDICT_FALSE(error_value)) {
    return error_value->ToStatus();
  }
  if (auto unknown_value = value.AsUnknown();
      ABSL_PREDICT_FALSE(unknown_value)) {
    return absl::InvalidArgumentError("cannot add unknown value to list");
  }
  return absl::OkStatus();
}

template <typename Vector>
absl::Status ListValueToJsonArray(
    const Vector& vector,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

  ListValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(json->GetDescriptor()));

  json->Clear();

  if (vector.empty()) {
    return absl::OkStatus();
  }

  for (const auto& element : vector) {
    CEL_RETURN_IF_ERROR(element->ConvertToJson(descriptor_pool, message_factory,
                                               reflection.AddValues(json)));
  }
  return absl::OkStatus();
}

template <typename Vector>
absl::Status ListValueToJson(
    const Vector& vector,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(json->GetDescriptor()));
  return ListValueToJsonArray(vector, descriptor_pool, message_factory,
                              reflection.MutableListValue(json));
}

class CompatListValueImplIterator final : public ValueIterator {
 public:
  explicit CompatListValueImplIterator(absl::Span<const Value> elements)
      : elements_(elements) {}

  bool HasNext() override { return index_ < elements_.size(); }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(index_ >= elements_.size())) {
      return absl::FailedPreconditionError(
          "ValueManager::Next called after ValueManager::HasNext returned "
          "false");
    }
    *result = elements_[index_++];
    return absl::OkStatus();
  }

  absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull key_or_value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key_or_value != nullptr);

    if (index_ >= elements_.size()) {
      return false;
    }
    *key_or_value = elements_[index_];
    ++index_;
    return true;
  }

  absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key,
      Value* absl_nullable value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key != nullptr);

    if (index_ >= elements_.size()) {
      return false;
    }
    if (value != nullptr) {
      *value = elements_[index_];
    }
    *key = IntValue(index_++);
    return true;
  }

 private:
  const absl::Span<const Value> elements_;
  size_t index_ = 0;
};

struct ValueFormatter {
  void operator()(std::string* out,
                  const std::pair<const Value, Value>& value) const {
    (*this)(out, value.first);
    out->append(": ");
    (*this)(out, value.second);
  }

  void operator()(std::string* out, const Value& value) const {
    out->append(value.DebugString());
  }
};

class ListValueBuilderImpl final : public ListValueBuilder {
 public:
  explicit ListValueBuilderImpl(google::protobuf::Arena* absl_nonnull arena)
      : arena_(arena) {
    elements_.Construct(arena);
  }

  ~ListValueBuilderImpl() override {
    if (!elements_trivially_destructible_) {
      elements_.Destruct();
    }
  }

  absl::Status Add(Value value) override {
    CEL_RETURN_IF_ERROR(CheckListElement(value));
    UnsafeAdd(std::move(value));
    return absl::OkStatus();
  }

  void UnsafeAdd(Value value) override {
    ABSL_DCHECK_OK(CheckListElement(value));
    elements_->emplace_back(std::move(value));
    if (elements_trivially_destructible_) {
      elements_trivially_destructible_ =
          ArenaTraits<>::trivially_destructible(elements_->back());
    }
  }

  size_t Size() const override { return elements_->size(); }

  void Reserve(size_t capacity) override { elements_->reserve(capacity); }

  ListValue Build() && override;

  CustomListValue BuildCustom() &&;

  const CompatListValue* absl_nonnull BuildCompat() &&;

  const CompatListValue* absl_nonnull BuildCompatAt(
      void* absl_nonnull address) &&;

 private:
  google::protobuf::Arena* absl_nonnull const arena_;
  internal::Manual<ValueVector> elements_;
  bool elements_trivially_destructible_ = true;
};

class CompatListValueImpl final : public CompatListValue {
 public:
  explicit CompatListValueImpl(ValueVector&& elements)
      : elements_(std::move(elements)) {}

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", ", ValueFormatter{}),
                        "]");
  }

  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    return ListValueToJsonArray(elements_, descriptor_pool, message_factory,
                                json);
  }

  CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    ABSL_DCHECK(arena != nullptr);

    ListValueBuilderImpl builder(arena);
    builder.Reserve(elements_.size());
    for (const auto& element : elements_) {
      builder.UnsafeAdd(element.Clone(arena));
    }
    return std::move(builder).BuildCustom();
  }

  size_t Size() const override { return elements_.size(); }

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    const size_t size = elements_.size();
    for (size_t i = 0; i < size; ++i) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(i, elements_[i]));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const override {
    return std::make_unique<CompatListValueImplIterator>(
        absl::MakeConstSpan(elements_));
  }

  CelValue operator[](int index) const override {
    return Get(elements_.get_allocator().arena(), index);
  }

  // Like `operator[](int)` above, but also accepts an arena. Prefer calling
  // this variant if the arena is known.
  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      arena = elements_.get_allocator().arena();
    }
    if (ABSL_PREDICT_FALSE(index < 0 || index >= size())) {
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, IndexOutOfBoundsError(index).ToStatus()));
    }
    return common_internal::UnsafeLegacyValue(
        elements_[index],
        /*stable=*/true,
        arena != nullptr ? arena : elements_.get_allocator().arena());
  }

  int size() const override { return static_cast<int>(Size()); }

 protected:
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const override {
    if (index >= elements_.size()) {
      *result = IndexOutOfBoundsError(index);
    } else {
      *result = elements_[index];
    }
    return absl::OkStatus();
  }

 private:
  const ValueVector elements_;
};

}  // namespace

}  // namespace common_internal

template <>
struct ArenaTraits<common_internal::CompatListValueImpl> {
  using always_trivially_destructible = std::true_type;
};

namespace common_internal {

namespace {

ListValue ListValueBuilderImpl::Build() && {
  if (elements_->empty()) {
    return ListValue();
  }
  return std::move(*this).BuildCustom();
}

CustomListValue ListValueBuilderImpl::BuildCustom() && {
  if (elements_->empty()) {
    return CustomListValue(EmptyCompatListValue(), arena_);
  }
  return CustomListValue(std::move(*this).BuildCompat(), arena_);
}

const CompatListValue* absl_nonnull ListValueBuilderImpl::BuildCompat() && {
  if (elements_->empty()) {
    return EmptyCompatListValue();
  }
  return std::move(*this).BuildCompatAt(arena_->AllocateAligned(
      sizeof(CompatListValueImpl), alignof(CompatListValueImpl)));
}

const CompatListValue* absl_nonnull ListValueBuilderImpl::BuildCompatAt(
    void* absl_nonnull address) && {
  CompatListValueImpl* absl_nonnull impl =
      ::new (address) CompatListValueImpl(std::move(*elements_));
  if (!elements_trivially_destructible_) {
    arena_->OwnDestructor(impl);
    elements_trivially_destructible_ = true;
  }
  return impl;
}

class MutableCompatListValueImpl final : public MutableCompatListValue {
 public:
  explicit MutableCompatListValueImpl(google::protobuf::Arena* absl_nonnull arena)
      : elements_(arena) {}

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", ", ValueFormatter{}),
                        "]");
  }

  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    return ListValueToJsonArray(elements_, descriptor_pool, message_factory,
                                json);
  }

  CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    ABSL_DCHECK(arena != nullptr);

    ListValueBuilderImpl builder(arena);
    builder.Reserve(elements_.size());
    for (const auto& element : elements_) {
      builder.UnsafeAdd(element.Clone(arena));
    }
    return std::move(builder).BuildCustom();
  }

  size_t Size() const override { return elements_.size(); }

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    const size_t size = elements_.size();
    for (size_t i = 0; i < size; ++i) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(i, elements_[i]));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const override {
    return std::make_unique<CompatListValueImplIterator>(
        absl::MakeConstSpan(elements_));
  }

  CelValue operator[](int index) const override {
    return Get(elements_.get_allocator().arena(), index);
  }

  // Like `operator[](int)` above, but also accepts an arena. Prefer calling
  // this variant if the arena is known.
  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      arena = elements_.get_allocator().arena();
    }
    if (ABSL_PREDICT_FALSE(index < 0 || index >= size())) {
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, IndexOutOfBoundsError(index).ToStatus()));
    }
    return common_internal::UnsafeLegacyValue(
        elements_[index], /*stable=*/false,
        arena != nullptr ? arena : elements_.get_allocator().arena());
  }

  int size() const override { return static_cast<int>(Size()); }

  absl::Status Append(Value value) const override {
    CEL_RETURN_IF_ERROR(CheckListElement(value));
    elements_.emplace_back(std::move(value));
    if (elements_trivially_destructible_) {
      elements_trivially_destructible_ =
          ArenaTraits<>::trivially_destructible(elements_.back());
      if (!elements_trivially_destructible_) {
        elements_.get_allocator().arena()->OwnDestructor(
            const_cast<MutableCompatListValueImpl*>(this));
      }
    }
    return absl::OkStatus();
  }

  void Reserve(size_t capacity) const override { elements_.reserve(capacity); }

 protected:
  absl::Status Get(size_t index,
                   const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                   google::protobuf::MessageFactory* absl_nonnull message_factory,
                   google::protobuf::Arena* absl_nonnull arena,
                   Value* absl_nonnull result) const override {
    if (index >= elements_.size()) {
      *result = IndexOutOfBoundsError(index);
    } else {
      *result = elements_[index];
    }
    return absl::OkStatus();
  }

 private:
  mutable ValueVector elements_;
  mutable bool elements_trivially_destructible_ = true;
};

}  // namespace

}  // namespace common_internal

template <>
struct ArenaTraits<common_internal::MutableCompatListValueImpl> {
  using constructible = std::true_type;

  using always_trivially_destructible = std::true_type;
};

namespace common_internal {

namespace {}  // namespace

absl::StatusOr<const CompatListValue* absl_nonnull> MakeCompatListValue(
    const CustomListValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  ListValueBuilderImpl builder(arena);
  builder.Reserve(value.Size());

  CEL_RETURN_IF_ERROR(value.ForEach(
      [&](const Value& element) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(builder.Add(element));
        return true;
      },
      descriptor_pool, message_factory, arena));

  return std::move(builder).BuildCompat();
}

MutableListValue* absl_nonnull NewMutableListValue(
    google::protobuf::Arena* absl_nonnull arena) {
  return ::new (arena->AllocateAligned(sizeof(MutableCompatListValueImpl),
                                       alignof(MutableCompatListValueImpl)))
      MutableCompatListValueImpl(arena);
}

bool IsMutableListValue(const Value& value) {
  if (auto custom_list_value = value.AsCustomList(); custom_list_value) {
    NativeTypeId native_type_id = custom_list_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableListValue>() ||
        native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return true;
    }
  }
  return false;
}

bool IsMutableListValue(const ListValue& value) {
  if (auto custom_list_value = value.AsCustom(); custom_list_value) {
    NativeTypeId native_type_id = custom_list_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableListValue>() ||
        native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return true;
    }
  }
  return false;
}

const MutableListValue* absl_nullable AsMutableListValue(const Value& value) {
  if (auto custom_list_value = value.AsCustomList(); custom_list_value) {
    NativeTypeId native_type_id = custom_list_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableListValue>()) {
      return cel::internal::down_cast<const MutableListValue*>(
          custom_list_value->interface());
    }
    if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return cel::internal::down_cast<const MutableCompatListValue*>(
          custom_list_value->interface());
    }
  }
  return nullptr;
}

const MutableListValue* absl_nullable AsMutableListValue(
    const ListValue& value) {
  if (auto custom_list_value = value.AsCustom(); custom_list_value) {
    NativeTypeId native_type_id = custom_list_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableListValue>()) {
      return cel::internal::down_cast<const MutableListValue*>(
          custom_list_value->interface());
    }
    if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return cel::internal::down_cast<const MutableCompatListValue*>(
          custom_list_value->interface());
    }
  }
  return nullptr;
}

const MutableListValue& GetMutableListValue(const Value& value) {
  ABSL_DCHECK(IsMutableListValue(value)) << value;
  const auto& custom_list_value = value.GetCustomList();
  NativeTypeId native_type_id = custom_list_value.GetTypeId();
  if (native_type_id == NativeTypeId::For<MutableListValue>()) {
    return cel::internal::down_cast<const MutableListValue&>(
        *custom_list_value.interface());
  }
  if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
    return cel::internal::down_cast<const MutableCompatListValue&>(
        *custom_list_value.interface());
  }
  ABSL_UNREACHABLE();
}

const MutableListValue& GetMutableListValue(const ListValue& value) {
  ABSL_DCHECK(IsMutableListValue(value)) << value;
  const auto& custom_list_value = value.GetCustom();
  NativeTypeId native_type_id = custom_list_value.GetTypeId();
  if (native_type_id == NativeTypeId::For<MutableListValue>()) {
    return cel::internal::down_cast<const MutableListValue&>(
        *custom_list_value.interface());
  }
  if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
    return cel::internal::down_cast<const MutableCompatListValue&>(
        *custom_list_value.interface());
  }
  ABSL_UNREACHABLE();
}

absl_nonnull cel::ListValueBuilderPtr NewListValueBuilder(
    google::protobuf::Arena* absl_nonnull arena) {
  return std::make_unique<ListValueBuilderImpl>(arena);
}

}  // namespace common_internal

}  // namespace cel

namespace cel {

namespace common_internal {

namespace {

using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelValue;

absl::Status CheckMapValue(const Value& value) {
  if (auto error_value = value.AsError(); ABSL_PREDICT_FALSE(error_value)) {
    return error_value->ToStatus();
  }
  if (auto unknown_value = value.AsUnknown();
      ABSL_PREDICT_FALSE(unknown_value)) {
    return absl::InvalidArgumentError("cannot add unknown value to list");
  }
  return absl::OkStatus();
}

size_t ValueHash(const Value& value) {
  switch (value.kind()) {
    case ValueKind::kBool:
      return absl::HashOf(value.kind(), value.GetBool());
    case ValueKind::kInt:
      return absl::HashOf(ValueKind::kInt,
                          absl::implicit_cast<int64_t>(value.GetInt()));
    case ValueKind::kUint:
      return absl::HashOf(ValueKind::kUint,
                          absl::implicit_cast<uint64_t>(value.GetUint()));
    case ValueKind::kString:
      return absl::HashOf(value.kind(), value.GetString());
    default:
      ABSL_UNREACHABLE();
  }
}

size_t ValueHash(const CelValue& value) {
  switch (value.type()) {
    case CelValue::Type::kBool:
      return absl::HashOf(ValueKind::kBool, value.BoolOrDie());
    case CelValue::Type::kInt:
      return absl::HashOf(ValueKind::kInt, value.Int64OrDie());
    case CelValue::Type::kUint:
      return absl::HashOf(ValueKind::kUint, value.Uint64OrDie());
    case CelValue::Type::kString:
      return absl::HashOf(ValueKind::kString, value.StringOrDie().value());
    default:
      ABSL_UNREACHABLE();
  }
}

bool ValueEquals(const Value& lhs, const Value& rhs) {
  switch (lhs.kind()) {
    case ValueKind::kBool:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return lhs.GetBool() == rhs.GetBool();
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case ValueKind::kInt:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return lhs.GetInt() == rhs.GetInt();
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case ValueKind::kUint:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return lhs.GetUint() == rhs.GetUint();
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case ValueKind::kString:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return lhs.GetString() == rhs.GetString();
        default:
          ABSL_UNREACHABLE();
      }
    default:
      ABSL_UNREACHABLE();
  }
}

bool CelValueEquals(const CelValue& lhs, const Value& rhs) {
  switch (lhs.type()) {
    case CelValue::Type::kBool:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return BoolValue(lhs.BoolOrDie()) == rhs.GetBool();
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case CelValue::Type::kInt:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return IntValue(lhs.Int64OrDie()) == rhs.GetInt();
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case CelValue::Type::kUint:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return UintValue(lhs.Uint64OrDie()) == rhs.GetUint();
        case ValueKind::kString:
          return false;
        default:
          ABSL_UNREACHABLE();
      }
    case CelValue::Type::kString:
      switch (rhs.kind()) {
        case ValueKind::kBool:
          return false;
        case ValueKind::kInt:
          return false;
        case ValueKind::kUint:
          return false;
        case ValueKind::kString:
          return rhs.GetString().Equals(lhs.StringOrDie().value());
        default:
          ABSL_UNREACHABLE();
      }
    default:
      ABSL_UNREACHABLE();
  }
}

absl::StatusOr<std::string> ValueToJsonString(const Value& value) {
  switch (value.kind()) {
    case ValueKind::kString:
      return value.GetString().NativeString();
    default:
      return TypeConversionError(value.GetRuntimeType(), StringType())
          .ToStatus();
  }
}

template <typename Map>
absl::Status MapValueToJsonObject(
    const Map& map, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  StructReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(json->GetDescriptor()));

  json->Clear();

  if (map.empty()) {
    return absl::OkStatus();
  }

  for (const auto& entry : map) {
    CEL_ASSIGN_OR_RETURN(auto key, ValueToJsonString(entry.first));
    CEL_RETURN_IF_ERROR(entry.second.ConvertToJson(
        descriptor_pool, message_factory, reflection.InsertField(json, key)));
  }
  return absl::OkStatus();
}

template <typename Map>
absl::Status MapValueToJson(
    const Map& map, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(json->GetDescriptor()));
  return MapValueToJsonObject(map, descriptor_pool, message_factory,
                              reflection.MutableStructValue(json));
}

struct ValueHasher {
  using is_transparent = void;

  size_t operator()(const Value& value) const { return (ValueHash)(value); }

  size_t operator()(const CelValue& value) const { return (ValueHash)(value); }
};

struct ValueEqualer {
  using is_transparent = void;

  bool operator()(const Value& lhs, const CelValue& rhs) const {
    return (*this)(rhs, lhs);
  }

  bool operator()(const CelValue& lhs, const Value& rhs) const {
    return (CelValueEquals)(lhs, rhs);
  }

  bool operator()(const Value& lhs, const Value& rhs) const {
    return (ValueEquals)(lhs, rhs);
  }
};

using ValueFlatHashMapAllocator = ArenaAllocator<std::pair<const Value, Value>>;

using ValueFlatHashMap =
    absl::flat_hash_map<Value, Value, ValueHasher, ValueEqualer,
                        ValueFlatHashMapAllocator>;

class CompatMapValueImplIterator final : public ValueIterator {
 public:
  explicit CompatMapValueImplIterator(const ValueFlatHashMap* absl_nonnull map)
      : begin_(map->begin()), end_(map->end()) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueManager::Next called after ValueManager::HasNext returned "
          "false");
    }
    *result = begin_->first;
    ++begin_;
    return absl::OkStatus();
  }

  absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull key_or_value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key_or_value != nullptr);

    if (begin_ == end_) {
      return false;
    }
    *key_or_value = begin_->first;
    ++begin_;
    return true;
  }

  absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key,
      Value* absl_nullable value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key != nullptr);

    if (begin_ == end_) {
      return false;
    }
    *key = begin_->first;
    if (value != nullptr) {
      *value = begin_->second;
    }
    ++begin_;
    return true;
  }

 private:
  typename ValueFlatHashMap::const_iterator begin_;
  const typename ValueFlatHashMap::const_iterator end_;
};

class MapValueBuilderImpl final : public MapValueBuilder {
 public:
  explicit MapValueBuilderImpl(google::protobuf::Arena* absl_nonnull arena)
      : arena_(arena) {
    map_.Construct(arena_);
  }

  ~MapValueBuilderImpl() override {
    if (!entries_trivially_destructible_) {
      map_.Destruct();
    }
  }

  absl::Status Put(Value key, Value value) override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    CEL_RETURN_IF_ERROR(CheckMapValue(value));
    if (auto it = map_->find(key); ABSL_PREDICT_FALSE(it != map_->end())) {
      return DuplicateKeyError().ToStatus();
    }
    UnsafePut(std::move(key), std::move(value));
    return absl::OkStatus();
  }

  void UnsafePut(Value key, Value value) override {
    auto insertion = map_->insert({std::move(key), std::move(value)});
    ABSL_DCHECK(insertion.second);
    if (entries_trivially_destructible_) {
      entries_trivially_destructible_ =
          ArenaTraits<>::trivially_destructible(insertion.first->first) &&
          ArenaTraits<>::trivially_destructible(insertion.first->second);
    }
  }

  size_t Size() const override { return map_->size(); }

  void Reserve(size_t capacity) override { map_->reserve(capacity); }

  MapValue Build() && override;

  CustomMapValue BuildCustom() &&;

  const CompatMapValue* absl_nonnull BuildCompat() &&;

 private:
  google::protobuf::Arena* absl_nonnull const arena_;
  internal::Manual<ValueFlatHashMap> map_;
  bool entries_trivially_destructible_ = true;
};

class CompatMapValueImpl final : public CompatMapValue {
 public:
  explicit CompatMapValueImpl(ValueFlatHashMap&& map) : map_(std::move(map)) {}

  std::string DebugString() const override {
    return absl::StrCat("{", absl::StrJoin(map_, ", ", ValueFormatter{}), "}");
  }

  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    return MapValueToJsonObject(map_, descriptor_pool, message_factory, json);
  }

  CustomMapValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    ABSL_DCHECK(arena != nullptr);

    MapValueBuilderImpl builder(arena);
    builder.Reserve(map_.size());
    for (const auto& entry : map_) {
      builder.UnsafePut(entry.first.Clone(arena), entry.second.Clone(arena));
    }
    return std::move(builder).BuildCustom();
  }

  size_t Size() const override { return map_.size(); }

  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      ListValue* absl_nonnull result) const override {
    *result = CustomListValue(ProjectKeys(), map_.get_allocator().arena());
    return absl::OkStatus();
  }

  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    for (const auto& entry : map_) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(entry.first, entry.second));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const override {
    return std::make_unique<CompatMapValueImplIterator>(&map_);
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    return Get(map_.get_allocator().arena(), key);
  }

  using CompatMapValue::Get;
  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    if (auto status = CelValue::CheckMapKeyType(key); !status.ok()) {
      status.IgnoreError();
      return absl::nullopt;
    }
    if (auto it = map_.find(key); it != map_.end()) {
      return common_internal::UnsafeLegacyValue(
          it->second, /*stable=*/true,
          arena != nullptr ? arena : map_.get_allocator().arena());
    }
    return absl::nullopt;
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    // This check safeguards against issues with invalid key types such as NaN.
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(key));
    return map_.find(key) != map_.end();
  }

  int size() const override { return static_cast<int>(Size()); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return ProjectKeys();
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena* arena) const override {
    return ProjectKeys();
  }

 protected:
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    if (auto it = map_.find(key); it != map_.end()) {
      *result = it->second;
      return true;
    }
    return false;
  }

  absl::StatusOr<bool> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    return map_.find(key) != map_.end();
  }

 private:
  const CompatListValue* absl_nonnull ProjectKeys() const {
    absl::call_once(keys_once_, [this]() {
      ListValueBuilderImpl builder(map_.get_allocator().arena());
      builder.Reserve(map_.size());

      for (const auto& entry : map_) {
        builder.UnsafeAdd(entry.first);
      }

      std::move(builder).BuildCompatAt(&keys_[0]);
    });
    return std::launder(
        reinterpret_cast<const CompatListValueImpl*>(&keys_[0]));
  }

  const ValueFlatHashMap map_;
  mutable absl::once_flag keys_once_;
  alignas(CompatListValueImpl) mutable char keys_[sizeof(CompatListValueImpl)];
};

MapValue MapValueBuilderImpl::Build() && {
  if (map_->empty()) {
    return MapValue();
  }
  return std::move(*this).BuildCustom();
}

CustomMapValue MapValueBuilderImpl::BuildCustom() && {
  if (map_->empty()) {
    return CustomMapValue(EmptyCompatMapValue(), arena_);
  }
  return CustomMapValue(std::move(*this).BuildCompat(), arena_);
}

const CompatMapValue* absl_nonnull MapValueBuilderImpl::BuildCompat() && {
  if (map_->empty()) {
    return EmptyCompatMapValue();
  }
  CompatMapValueImpl* absl_nonnull impl = ::new (arena_->AllocateAligned(
      sizeof(CompatMapValueImpl), alignof(CompatMapValueImpl)))
      CompatMapValueImpl(std::move(*map_));
  if (!entries_trivially_destructible_) {
    arena_->OwnDestructor(impl);
    entries_trivially_destructible_ = true;
  }
  return impl;
}

class TrivialMutableMapValueImpl final : public MutableCompatMapValue {
 public:
  explicit TrivialMutableMapValueImpl(google::protobuf::Arena* absl_nonnull arena)
      : map_(arena) {}

  std::string DebugString() const override {
    return absl::StrCat("{", absl::StrJoin(map_, ", ", ValueFormatter{}), "}");
  }

  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    return MapValueToJsonObject(map_, descriptor_pool, message_factory, json);
  }

  CustomMapValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    ABSL_DCHECK(arena != nullptr);

    MapValueBuilderImpl builder(arena);
    builder.Reserve(map_.size());
    for (const auto& entry : map_) {
      builder.UnsafePut(entry.first.Clone(arena), entry.second.Clone(arena));
    }
    return std::move(builder).BuildCustom();
  }

  size_t Size() const override { return map_.size(); }

  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      ListValue* absl_nonnull result) const override {
    *result = CustomListValue(ProjectKeys(), map_.get_allocator().arena());
    return absl::OkStatus();
  }

  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    for (const auto& entry : map_) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(entry.first, entry.second));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const override {
    return std::make_unique<CompatMapValueImplIterator>(&map_);
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    return Get(map_.get_allocator().arena(), key);
  }

  using MutableCompatMapValue::Get;
  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    if (auto status = CelValue::CheckMapKeyType(key); !status.ok()) {
      status.IgnoreError();
      return absl::nullopt;
    }
    if (auto it = map_.find(key); it != map_.end()) {
      return common_internal::UnsafeLegacyValue(
          it->second, /*stable=*/false,
          arena != nullptr ? arena : map_.get_allocator().arena());
    }
    return absl::nullopt;
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    // This check safeguards against issues with invalid key types such as NaN.
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(key));
    return map_.find(key) != map_.end();
  }

  int size() const override { return static_cast<int>(Size()); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return ProjectKeys();
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena* arena) const override {
    return ProjectKeys();
  }

  absl::Status Put(Value key, Value value) const override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    CEL_RETURN_IF_ERROR(CheckMapValue(value));
    if (auto it = map_.find(key); ABSL_PREDICT_FALSE(it != map_.end())) {
      return DuplicateKeyError().ToStatus();
    }
    auto insertion = map_.insert({std::move(key), std::move(value)});
    ABSL_DCHECK(insertion.second);
    if (entries_trivially_destructible_) {
      entries_trivially_destructible_ =
          ArenaTraits<>::trivially_destructible(insertion.first->first) &&
          ArenaTraits<>::trivially_destructible(insertion.first->second);
      if (!entries_trivially_destructible_) {
        map_.get_allocator().arena()->OwnDestructor(
            const_cast<TrivialMutableMapValueImpl*>(this));
      }
    }
    return absl::OkStatus();
  }

  void Reserve(size_t capacity) const override { map_.reserve(capacity); }

 protected:
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    if (auto it = map_.find(key); it != map_.end()) {
      *result = it->second;
      return true;
    }
    return false;
  }

  absl::StatusOr<bool> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    CEL_RETURN_IF_ERROR(CheckMapKey(key));
    return map_.find(key) != map_.end();
  }

 private:
  const CompatListValue* absl_nonnull ProjectKeys() const {
    absl::call_once(keys_once_, [this]() {
      ListValueBuilderImpl builder(map_.get_allocator().arena());
      builder.Reserve(map_.size());

      for (const auto& entry : map_) {
        builder.UnsafeAdd(entry.first);
      }

      std::move(builder).BuildCompatAt(&keys_[0]);
    });
    return std::launder(
        reinterpret_cast<const CompatListValueImpl*>(&keys_[0]));
  }

  mutable ValueFlatHashMap map_;
  mutable bool entries_trivially_destructible_ = true;
  mutable absl::once_flag keys_once_;
  alignas(CompatListValueImpl) mutable char keys_[sizeof(CompatListValueImpl)];
};

}  // namespace

absl::StatusOr<const CompatMapValue* absl_nonnull> MakeCompatMapValue(
    const CustomMapValue& value,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  MapValueBuilderImpl builder(arena);
  builder.Reserve(value.Size());

  CEL_RETURN_IF_ERROR(value.ForEach(
      [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(builder.Put(key, value));
        return true;
      },
      descriptor_pool, message_factory, arena));

  return std::move(builder).BuildCompat();
}

MutableMapValue* absl_nonnull NewMutableMapValue(
    google::protobuf::Arena* absl_nonnull arena) {
  return ::new (arena->AllocateAligned(sizeof(TrivialMutableMapValueImpl),
                                       alignof(TrivialMutableMapValueImpl)))
      TrivialMutableMapValueImpl(arena);
}

bool IsMutableMapValue(const Value& value) {
  if (auto custom_map_value = value.AsCustomMap(); custom_map_value) {
    NativeTypeId native_type_id = custom_map_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableMapValue>() ||
        native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
      return true;
    }
  }
  return false;
}

bool IsMutableMapValue(const MapValue& value) {
  if (auto custom_map_value = value.AsCustom(); custom_map_value) {
    NativeTypeId native_type_id = custom_map_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableMapValue>() ||
        native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
      return true;
    }
  }
  return false;
}

const MutableMapValue* absl_nullable AsMutableMapValue(const Value& value) {
  if (auto custom_map_value = value.AsCustomMap(); custom_map_value) {
    NativeTypeId native_type_id = custom_map_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableMapValue>()) {
      return cel::internal::down_cast<const MutableMapValue*>(
          custom_map_value->interface());
    }
    if (native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
      return cel::internal::down_cast<const MutableCompatMapValue*>(
          custom_map_value->interface());
    }
  }
  return nullptr;
}

const MutableMapValue* absl_nullable AsMutableMapValue(const MapValue& value) {
  if (auto custom_map_value = value.AsCustom(); custom_map_value) {
    NativeTypeId native_type_id = custom_map_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<MutableMapValue>()) {
      return cel::internal::down_cast<const MutableMapValue*>(
          custom_map_value->interface());
    }
    if (native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
      return cel::internal::down_cast<const MutableCompatMapValue*>(
          custom_map_value->interface());
    }
  }
  return nullptr;
}

const MutableMapValue& GetMutableMapValue(const Value& value) {
  ABSL_DCHECK(IsMutableMapValue(value)) << value;
  const auto& custom_map_value = value.GetCustomMap();
  NativeTypeId native_type_id = custom_map_value.GetTypeId();
  if (native_type_id == NativeTypeId::For<MutableMapValue>()) {
    return cel::internal::down_cast<const MutableMapValue&>(
        *custom_map_value.interface());
  }
  if (native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
    return cel::internal::down_cast<const MutableCompatMapValue&>(
        *custom_map_value.interface());
  }
  ABSL_UNREACHABLE();
}

const MutableMapValue& GetMutableMapValue(const MapValue& value) {
  ABSL_DCHECK(IsMutableMapValue(value)) << value;
  const auto& custom_map_value = value.GetCustom();
  NativeTypeId native_type_id = custom_map_value.GetTypeId();
  if (native_type_id == NativeTypeId::For<MutableMapValue>()) {
    return cel::internal::down_cast<const MutableMapValue&>(
        *custom_map_value.interface());
  }
  if (native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
    return cel::internal::down_cast<const MutableCompatMapValue&>(
        *custom_map_value.interface());
  }
  ABSL_UNREACHABLE();
}

absl_nonnull cel::MapValueBuilderPtr NewMapValueBuilder(
    google::protobuf::Arena* absl_nonnull arena) {
  return std::make_unique<MapValueBuilderImpl>(arena);
}

}  // namespace common_internal

}  // namespace cel
