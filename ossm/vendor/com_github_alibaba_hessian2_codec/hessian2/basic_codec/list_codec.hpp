#pragma once

#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/basic_codec/type_ref_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<TypedListObject> Decoder::decode();

template <>
std::unique_ptr<UntypedListObject> Decoder::decode();

template <>
bool Encoder::encode(const TypedListObject& value);

template <>
bool Encoder::encode(const UntypedListObject& value);

}  // namespace Hessian2
