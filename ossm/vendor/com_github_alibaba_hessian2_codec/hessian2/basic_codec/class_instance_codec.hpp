#pragma once

#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/object_codec.hpp"

#include "hessian2/basic_codec/def_ref_codec.hpp"
#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/basic_codec/string_codec.hpp"
#include "hessian2/basic_codec/type_ref_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<ClassInstanceObject> Decoder::decode();

template <>
bool Encoder::encode(const ClassInstanceObject& value);

}  // namespace Hessian2
