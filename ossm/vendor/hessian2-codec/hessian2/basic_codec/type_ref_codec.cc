#include "hessian2/basic_codec/type_ref_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<Object::TypeRef> Decoder::decode() {
  auto ret = reader_->peek<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }
  auto code = ret.second;
  if (code <= 31 || (code >= 48 && code <= 51) || code == 82 || code == 83) {
    // String type
    auto type_str = decode<std::string>();
    if (!type_str) {
      return nullptr;
    }
    types_ref_.push_back(*type_str);
    return std::make_unique<Object::TypeRef>(*type_str);
  }

  // int32_t
  auto ret_int = decode<int32_t>();
  if (!ret_int) {
    return nullptr;
  }

  auto ref = static_cast<uint32_t>(*ret_int);
  if (types_ref_.size() <= ref) {
    return nullptr;
  } else {
    return std::make_unique<Object::TypeRef>(types_ref_[ref]);
  }

  return nullptr;
}

template <>
bool Encoder::encode(const Object::TypeRef &value) {
  auto r = getTypeRef(value.type_);
  if (r == -1) {
    types_ref_.emplace(value.type_, types_ref_.size());
    encode<std::string>(value.type_);
  } else {
    encode<int32_t>(r);
  }

  return true;
}

}  // namespace Hessian2
