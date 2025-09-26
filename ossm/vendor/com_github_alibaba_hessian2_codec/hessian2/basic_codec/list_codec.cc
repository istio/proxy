#include "hessian2/basic_codec/list_codec.hpp"

namespace Hessian2 {

// # typed list/vector
// ::= x55 type value* 'Z'   # variable-length list
// ::= 'V' type int value*   # fixed-length list
// ::= [x70-77] type value*  # fixed-length typed list
template <>
std::unique_ptr<TypedListObject> Decoder::decode() {
  std::string type;
  Object::TypedList obj_list;

  auto result = std::make_unique<TypedListObject>();
  values_ref_.push_back(result.get());

  auto ret = reader_->read<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }

  auto code = ret.second;

  auto ReadNumsObject = [&](int nums) -> bool {
    for (int i = 0; i < nums; i++) {
      auto o = decode<Object>();
      if (!o) {
        return false;
      }
      obj_list.values_.emplace_back(std::move(o));
    }
    return true;
  };

  auto ReadObjectUntilEnd = [&]() -> bool {
    auto ret = reader_->peek<uint8_t>();
    if (!ret.first) {
      return false;
    }

    while (ret.second != 'Z') {
      auto o = decode<Object>();
      if (!o) {
        return false;
      }
      obj_list.values_.emplace_back(std::move(o));
      ret = reader_->peek<uint8_t>();
      if (!ret.first) {
        return false;
      }
    }

    // Skip last 'Z'
    reader_->read<uint8_t>();
    return true;
  };

  auto type_str = decode<Object::TypeRef>();
  if (!type_str) {
    return nullptr;
  }

  obj_list.type_name_ = std::move(type_str->type_);

  switch (code) {
    case 0x55: {
      if (!ReadObjectUntilEnd()) {
        return nullptr;
      }
      break;
    }

    case 'V': {
      auto ret = decode<int32_t>();
      if (!ret) {
        return nullptr;
      }

      if (!ReadNumsObject(*ret)) {
        return nullptr;
      }
      break;
    }
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77: {
      if (!ReadNumsObject(code - 0x70)) {
        return nullptr;
      }
      break;
    }
    default:
      return nullptr;
  }
  result->setTypedList(std::move(obj_list));
  return result;
}

// ::= x57 value* 'Z'        # variable-length untyped list
// ::= x58 int value*        # fixed-length untyped list
// ::= [x78-7f] value*       # fixed-length untyped list
template <>
std::unique_ptr<UntypedListObject> Decoder::decode() {
  Object::UntypedList obj_list;

  auto result = std::make_unique<UntypedListObject>();
  values_ref_.push_back(result.get());

  auto ret = reader_->read<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }

  auto code = ret.second;

  auto ReadNumsObject = [&](int nums) -> bool {
    for (int i = 0; i < nums; i++) {
      auto o = decode<Object>();
      if (!o) {
        return false;
      }
      obj_list.emplace_back(std::move(o));
    }
    return true;
  };

  auto ReadObjectUntilEnd = [&]() -> bool {
    auto ret = reader_->peek<uint8_t>();
    if (!ret.first) {
      return false;
    }

    while (ret.second != 'Z') {
      auto o = decode<Object>();
      if (!o) {
        return false;
      }
      obj_list.emplace_back(std::move(o));
      ret = reader_->peek<uint8_t>();
      if (!ret.first) {
        return false;
      }
    }
    // Skip last 'Z'
    reader_->read<uint8_t>();
    return true;
  };

  switch (code) {
    case 0x57: {
      if (!ReadObjectUntilEnd()) {
        return nullptr;
      }
      break;
    }
    case 0x58: {
      auto ret = decode<int32_t>();
      if (!ret) {
        return nullptr;
      }
      if (!ReadNumsObject(*ret)) {
        return nullptr;
      }
      break;
    }
    case 0x78:
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f: {
      if (!ReadNumsObject(code - 0x78)) {
        return nullptr;
      }
      break;
    }
  }
  result->setUntypedList(std::move(obj_list));
  return result;
}

template <>
bool Encoder::encode(const TypedListObject& value) {
  values_ref_.emplace(&value, values_ref_.size());
  auto typed_list = value.toTypedList();
  ABSL_ASSERT(typed_list.has_value());
  auto& typed_list_value = typed_list.value().get();

  Object::TypeRef type_ref(typed_list_value.type_name_);
  auto len = typed_list_value.values_.size();

  if (len <= 7) {
    writer_->writeByte(static_cast<uint8_t>(0x70 + len));
  } else {
    writer_->writeByte('V');
  }

  encode<Object::TypeRef>(type_ref);
  if (len > 7) {
    encode<int32_t>(len);
  }

  for (size_t i = 0; i < len; i++) {
    encode<Object>(*typed_list_value.values_[i]);
  }

  return true;
}

template <>
bool Encoder::encode(const UntypedListObject& value) {
  values_ref_.emplace(&value, values_ref_.size());
  auto untyped_list = value.toUntypedList();
  ABSL_ASSERT(untyped_list.has_value());
  auto& untyped_list_value = untyped_list.value().get();

  auto len = untyped_list_value.size();

  if (len <= 7) {
    writer_->writeByte(static_cast<uint8_t>(0x78 + len));
  } else {
    writer_->writeByte(static_cast<uint8_t>(0x58));
    encode<int32_t>(len);
  }

  for (size_t i = 0; i < len; i++) {
    encode<Object>(*(untyped_list_value)[i]);
  }

  return true;
}

}  // namespace Hessian2
