#include "hessian2/basic_codec/map_codec.hpp"

namespace Hessian2 {

//# map/object
// ::= 'M' type (value value)* 'Z'  # key, value map pairs
template <>
std::unique_ptr<TypedMapObject> Decoder::decode() {
  std::string type;
  Object::TypedMap obj_map;

  auto result = std::make_unique<TypedMapObject>();
  values_ref_.push_back(result.get());
  auto ret = reader_->read<uint8_t>();
  ABSL_ASSERT(ret.first);
  auto code = ret.second;
  ABSL_ASSERT(code == 'M');
  // Read Type
  auto type_str = decode<Object::TypeRef>();
  if (!type_str) {
    return nullptr;
  }

  obj_map.type_name_ = std::move(type_str->type_);

  ret = reader_->peek<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }

  while (ret.second != 'Z') {
    auto key = decode<Object>();
    if (!key) {
      return nullptr;
    }

    auto value = decode<Object>();
    if (!value) {
      return nullptr;
    }

    obj_map.field_name_and_value_.emplace(std::move(key), std::move(value));
    ret = reader_->peek<uint8_t>();
    if (!ret.first) {
      return nullptr;
    }
  }

  // Skip last 'Z'
  reader_->read<uint8_t>();
  result->setTypedMap(std::move(obj_map));
  return result;
}

// ::= 'H' (value value)* 'Z'       # untyped key, value
template <>
std::unique_ptr<UntypedMapObject> Decoder::decode() {
  std::string type;
  Object::UntypedMap obj_map;

  auto result = std::make_unique<UntypedMapObject>();
  values_ref_.push_back(result.get());
  auto ret = reader_->read<uint8_t>();
  ABSL_ASSERT(ret.first);
  auto code = ret.second;
  ABSL_ASSERT(code == 'H');

  ret = reader_->peek<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }

  while (ret.second != 'Z') {
    auto key = decode<Object>();
    if (!key) {
      return nullptr;
    }

    auto value = decode<Object>();
    if (!value) {
      return nullptr;
    }

    obj_map.emplace(std::move(key), std::move(value));
    ret = reader_->peek<uint8_t>();
    if (!ret.first) {
      return nullptr;
    }
  }

  // Skip last 'Z'
  reader_->read<uint8_t>();
  result->setUntypedMap(std::move(obj_map));
  return result;
}

template <>
bool Encoder::encode(const TypedMapObject& value) {
  values_ref_.emplace(&value, values_ref_.size());
  auto typed_map = value.toTypedMap();
  ABSL_ASSERT(typed_map.has_value());
  auto& typed_map_value = typed_map.value().get();
  writer_->writeByte('M');
  Object::TypeRef type_ref(typed_map_value.type_name_);
  encode<Object::TypeRef>(type_ref);
  for (const auto& elem : typed_map_value.field_name_and_value_) {
    encode<Object>(*elem.first);
    encode<Object>(*elem.second);
  }
  writer_->writeByte('Z');
  return true;
}

template <>
bool Encoder::encode(const UntypedMapObject& value) {
  values_ref_.emplace(&value, values_ref_.size());
  auto untyped_map = value.toUntypedMap();
  ABSL_ASSERT(untyped_map.has_value());
  auto& untyped_map_value = untyped_map.value().get();
  writer_->writeByte('H');
  for (const auto& elem : untyped_map_value) {
    encode<Object>(*elem.first);
    encode<Object>(*elem.second);
  }
  writer_->writeByte('Z');
  return true;
}

}  // namespace Hessian2
