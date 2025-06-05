#pragma once

#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/basic_codec/string_codec.hpp"
#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<Object::Definition> Decoder::decode();

// TODO(tianqian.zyf:) Avoid copying definitions
template <>
bool Encoder::encode(const Object::RawDefinition &value);

// TODO(tianqian.zyf):
template <>
bool Encoder::encode(const Object::Definition &value);

}  // namespace Hessian2
