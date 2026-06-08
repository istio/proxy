#include "hessian2/basic_codec/class_instance_codec.hpp"

namespace Hessian2 {

// class-def  ::= 'C' string int string*
// object     ::= 'O' int value*
//           ::= [x60-x6f] value*
template <>
std::unique_ptr<ClassInstanceObject> Decoder::decode() {
  Object::ClassInstance instance;
  auto result = std::make_unique<ClassInstanceObject>();
  values_ref_.push_back(result.get());
  auto ret = reader_->peek<uint8_t>();

  if (!ret.first) {
    return nullptr;
  }

  // Read the actual definition
  if (ret.second == 'C') {
    auto def = decode<Object::Definition>();
    if (!def) {
      return nullptr;
    }
  }

  // Find the definition by reference
  auto def = decode<Object::Definition>();
  if (!def) {
    return nullptr;
  }
  instance.def_ = def->data_;
  auto obj_len = def->data_->field_names_.size();
  for (size_t i = 0; i < obj_len; i++) {
    auto o = decode<Object>();
    if (!o) {
      return nullptr;
    }
    instance.data_.emplace_back(std::move(o));
  }
  result->setClassInstance(std::move(instance));
  return result;
}

template <>
bool Encoder::encode(const ClassInstanceObject& value) {
  values_ref_.emplace(&value, values_ref_.size());
  auto class_instance = value.toClassInstance();
  ABSL_ASSERT(class_instance.has_value());
  auto& class_instance_value = class_instance.value().get();

  encode<Object::RawDefinition>(*class_instance_value.def_);
  for (const auto& value : class_instance_value.data_) {
    encode<Object>(*value);
  }
  return true;
}

}  // namespace Hessian2
