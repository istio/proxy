#pragma once

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/basic_codec/bool_codec.hpp"
#include "hessian2/basic_codec/byte_codec.hpp"
#include "hessian2/basic_codec/class_instance_codec.hpp"
#include "hessian2/basic_codec/date_codec.hpp"
#include "hessian2/basic_codec/def_ref_codec.hpp"
#include "hessian2/basic_codec/list_codec.hpp"
#include "hessian2/basic_codec/map_codec.hpp"
#include "hessian2/basic_codec/number_codec.hpp"
#include "hessian2/basic_codec/ref_object_codec.hpp"
#include "hessian2/basic_codec/string_codec.hpp"
#include "hessian2/basic_codec/type_ref_codec.hpp"

namespace Hessian2 {
template <>
std::unique_ptr<NullObject> Decoder::decode();

template <>
bool Encoder::encode(const NullObject&);

template <>
std::unique_ptr<Object> Decoder::decode();

template <>
bool Encoder::encode(const Object& value);

}  // namespace Hessian2
