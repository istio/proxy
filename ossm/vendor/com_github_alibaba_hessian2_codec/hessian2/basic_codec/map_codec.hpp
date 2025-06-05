#pragma once

#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/basic_codec/string_codec.hpp"
#include "hessian2/basic_codec/type_ref_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<TypedMapObject> Decoder::decode();

template <>
std::unique_ptr<UntypedMapObject> Decoder::decode();

template <>
bool Encoder::encode(const TypedMapObject& value);

template <>
bool Encoder::encode(const UntypedMapObject& value);

}  // namespace Hessian2
