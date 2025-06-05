#include "tools/flatbuffers_backed_impl.h"

#include <algorithm>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "eval/public/cel_value.h"
#include "flatbuffers/flatbuffers.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

CelValue CreateValue(int64_t value) { return CelValue::CreateInt64(value); }
CelValue CreateValue(uint64_t value) { return CelValue::CreateUint64(value); }
CelValue CreateValue(double value) { return CelValue::CreateDouble(value); }
CelValue CreateValue(bool value) { return CelValue::CreateBool(value); }

template <typename T, typename U>
class FlatBuffersListImpl : public CelList {
 public:
  FlatBuffersListImpl(const flatbuffers::Table& table,
                      const reflection::Field& field)
      : list_(table.GetPointer<const flatbuffers::Vector<T>*>(field.offset())) {
  }
  int size() const override { return list_ ? list_->size() : 0; }
  CelValue operator[](int index) const override {
    return CreateValue(static_cast<U>(list_->Get(index)));
  }

 private:
  const flatbuffers::Vector<T>* list_;
};

class StringListImpl : public CelList {
 public:
  explicit StringListImpl(
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* list)
      : list_(list) {}
  int size() const override { return list_ ? list_->size() : 0; }
  CelValue operator[](int index) const override {
    auto value = list_->Get(index);
    return CelValue::CreateStringView(
        absl::string_view(value->c_str(), value->size()));
  }

 private:
  const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* list_;
};

class ObjectListImpl : public CelList {
 public:
  ObjectListImpl(
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>* list,
      const reflection::Schema& schema, const reflection::Object& object,
      google::protobuf::Arena* arena)
      : arena_(arena), list_(list), schema_(schema), object_(object) {}
  int size() const override { return list_ ? list_->size() : 0; }
  CelValue operator[](int index) const override {
    auto value = list_->Get(index);
    return CelValue::CreateMap(google::protobuf::Arena::Create<FlatBuffersMapImpl>(
        arena_, *value, schema_, object_, arena_));
  }

 private:
  google::protobuf::Arena* arena_;
  const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>* list_;
  const reflection::Schema& schema_;
  const reflection::Object& object_;
};

class ObjectStringIndexedMapImpl : public CelMap {
 public:
  ObjectStringIndexedMapImpl(
      const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>* list,
      const reflection::Schema& schema, const reflection::Object& object,
      const reflection::Field& index, google::protobuf::Arena* arena)
      : arena_(arena),
        list_(list),
        schema_(schema),
        object_(object),
        index_(index) {
    keys_.parent = this;
  }

  int size() const override { return list_ ? list_->size() : 0; }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    auto lookup_result = (*this)[key];
    if (!lookup_result.has_value()) {
      return false;
    }
    auto result = *lookup_result;
    if (result.IsError()) {
      return *(result.ErrorOrDie());
    }
    return true;
  }

  absl::optional<CelValue> operator[](CelValue cel_key) const override {
    if (!cel_key.IsString()) {
      return CreateErrorValue(
          arena_, absl::InvalidArgumentError(
                      absl::StrCat("Invalid map key type: '",
                                   CelValue::TypeName(cel_key.type()), "'")));
    }
    const absl::string_view key = cel_key.StringOrDie().value();
    const auto it = std::lower_bound(
        list_->begin(), list_->end(), key,
        [this](const flatbuffers::Table* t, const absl::string_view key) {
          auto value = flatbuffers::GetFieldS(*t, index_);
          auto sv = value ? absl::string_view(value->c_str(), value->size())
                          : absl::string_view();
          return sv < key;
        });
    if (it != list_->end()) {
      auto value = flatbuffers::GetFieldS(**it, index_);
      auto sv = value ? absl::string_view(value->c_str(), value->size())
                      : absl::string_view();
      if (sv == key) {
        return CelValue::CreateMap(google::protobuf::Arena::Create<FlatBuffersMapImpl>(
            arena_, **it, schema_, object_, arena_));
      }
    }
    return absl::nullopt;
  }

  absl::StatusOr<const CelList*> ListKeys() const override { return &keys_; }

 private:
  struct KeyList : public CelList {
    int size() const override { return parent->size(); }
    CelValue operator[](int index) const override {
      auto value =
          flatbuffers::GetFieldS(*(parent->list_->Get(index)), parent->index_);
      if (value == nullptr) {
        return CelValue::CreateStringView(absl::string_view());
      }
      return CelValue::CreateStringView(
          absl::string_view(value->c_str(), value->size()));
    }
    ObjectStringIndexedMapImpl* parent;
  };
  google::protobuf::Arena* arena_;
  const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::Table>>* list_;
  const reflection::Schema& schema_;
  const reflection::Object& object_;
  const reflection::Field& index_;
  KeyList keys_;
};

// Detects a "key" field of the type string.
const reflection::Field* findStringKeyField(const reflection::Object& object) {
  for (const auto field : *object.fields()) {
    if (field->key() && field->type()->base_type() == reflection::String) {
      return field;
    }
  }
  return nullptr;
}

}  // namespace

absl::StatusOr<bool> FlatBuffersMapImpl::Has(const CelValue& key) const {
  auto lookup_result = (*this)[key];
  if (!lookup_result.has_value()) {
    return false;
  }
  auto result = *lookup_result;
  if (result.IsError()) {
    return *(result.ErrorOrDie());
  }
  return true;
}

absl::optional<CelValue> FlatBuffersMapImpl::operator[](
    CelValue cel_key) const {
  if (!cel_key.IsString()) {
    return CreateErrorValue(
        arena_, absl::InvalidArgumentError(
                    absl::StrCat("Invalid map key type: '",
                                 CelValue::TypeName(cel_key.type()), "'")));
  }
  auto field = keys_.fields->LookupByKey(cel_key.StringOrDie().value().data());
  if (field == nullptr) {
    return absl::nullopt;
  }
  switch (field->type()->base_type()) {
    case reflection::Byte:
      return CelValue::CreateInt64(
          flatbuffers::GetFieldI<int8_t>(table_, *field));
    case reflection::Short:
      return CelValue::CreateInt64(
          flatbuffers::GetFieldI<int16_t>(table_, *field));
    case reflection::Int:
      return CelValue::CreateInt64(
          flatbuffers::GetFieldI<int32_t>(table_, *field));
    case reflection::Long:
      return CelValue::CreateInt64(
          flatbuffers::GetFieldI<int64_t>(table_, *field));
    case reflection::UByte:
      return CelValue::CreateUint64(
          flatbuffers::GetFieldI<uint8_t>(table_, *field));
    case reflection::UShort:
      return CelValue::CreateUint64(
          flatbuffers::GetFieldI<uint16_t>(table_, *field));
    case reflection::UInt:
      return CelValue::CreateUint64(
          flatbuffers::GetFieldI<uint32_t>(table_, *field));
    case reflection::ULong:
      return CelValue::CreateUint64(
          flatbuffers::GetFieldI<uint64_t>(table_, *field));
    case reflection::Float:
      return CelValue::CreateDouble(
          flatbuffers::GetFieldF<float>(table_, *field));
    case reflection::Double:
      return CelValue::CreateDouble(
          flatbuffers::GetFieldF<double>(table_, *field));
    case reflection::Bool:
      return CelValue::CreateBool(
          flatbuffers::GetFieldI<int8_t>(table_, *field));
    case reflection::String: {
      auto value = flatbuffers::GetFieldS(table_, *field);
      if (value == nullptr) {
        return CelValue::CreateStringView(absl::string_view());
      }
      return CelValue::CreateStringView(
          absl::string_view(value->c_str(), value->size()));
    }
    case reflection::Obj: {
      const auto* field_schema = schema_.objects()->Get(field->type()->index());
      const auto* field_table = flatbuffers::GetFieldT(table_, *field);
      if (field_table == nullptr) {
        return CelValue::CreateNull();
      }
      if (field_schema) {
        return CelValue::CreateMap(google::protobuf::Arena::Create<FlatBuffersMapImpl>(
            arena_, *field_table, schema_, *field_schema, arena_));
      }
      break;
    }
    case reflection::Vector: {
      switch (field->type()->element()) {
        case reflection::Byte:
        case reflection::UByte: {
          const auto* field_table = flatbuffers::GetFieldAnyV(table_, *field);
          if (field_table == nullptr) {
            return CelValue::CreateBytesView(absl::string_view());
          }
          return CelValue::CreateBytesView(absl::string_view(
              reinterpret_cast<const char*>(field_table->Data()),
              field_table->size()));
        }
        case reflection::Short:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<int16_t, int64_t>>(
                  arena_, table_, *field));
        case reflection::Int:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<int32_t, int64_t>>(
                  arena_, table_, *field));
        case reflection::Long:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<int64_t, int64_t>>(
                  arena_, table_, *field));
        case reflection::UShort:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<uint16_t, uint64_t>>(
                  arena_, table_, *field));
        case reflection::UInt:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<uint32_t, uint64_t>>(
                  arena_, table_, *field));
        case reflection::ULong:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<uint64_t, uint64_t>>(
                  arena_, table_, *field));
        case reflection::Float:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<float, double>>(
                  arena_, table_, *field));
        case reflection::Double:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<double, double>>(
                  arena_, table_, *field));
        case reflection::Bool:
          return CelValue::CreateList(
              google::protobuf::Arena::Create<FlatBuffersListImpl<uint8_t, bool>>(
                  arena_, table_, *field));
        case reflection::String:
          return CelValue::CreateList(google::protobuf::Arena::Create<StringListImpl>(
              arena_, table_.GetPointer<const flatbuffers::Vector<
                          flatbuffers::Offset<flatbuffers::String>>*>(
                          field->offset())));
        case reflection::Obj: {
          const auto* field_schema =
              schema_.objects()->Get(field->type()->index());
          if (field_schema) {
            const auto* index = findStringKeyField(*field_schema);
            if (index) {
              return CelValue::CreateMap(
                  google::protobuf::Arena::Create<ObjectStringIndexedMapImpl>(
                      arena_,
                      table_.GetPointer<const flatbuffers::Vector<
                          flatbuffers::Offset<flatbuffers::Table>>*>(
                          field->offset()),
                      schema_, *field_schema, *index, arena_));
            } else {
              return CelValue::CreateList(google::protobuf::Arena::Create<ObjectListImpl>(
                  arena_,
                  table_.GetPointer<const flatbuffers::Vector<
                      flatbuffers::Offset<flatbuffers::Table>>*>(
                      field->offset()),
                  schema_, *field_schema, arena_));
            }
          }
          break;
        }
        default:
          // Unsupported vector base types
          return absl::nullopt;
      }
      break;
    }
    default:
      // Unsupported types: enums, unions, arrays
      return absl::nullopt;
  }
  return absl::nullopt;
}

const CelMap* CreateFlatBuffersBackedObject(const uint8_t* flatbuf,
                                            const reflection::Schema& schema,
                                            google::protobuf::Arena* arena) {
  return google::protobuf::Arena::Create<const FlatBuffersMapImpl>(
      arena, *flatbuffers::GetAnyRoot(flatbuf), schema, *schema.root_table(),
      arena);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
