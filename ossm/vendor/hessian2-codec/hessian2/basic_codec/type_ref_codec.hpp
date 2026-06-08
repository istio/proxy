#pragma once

#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/basic_codec/string_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<Object::TypeRef> Decoder::decode();

template <>
bool Encoder::encode(const Object::TypeRef &value);

}  // namespace Hessian2
