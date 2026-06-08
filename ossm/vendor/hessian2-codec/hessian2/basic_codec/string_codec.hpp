#pragma once

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "hessian2/codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<std::string> Decoder::decode();

template <>
bool Encoder::encode(const absl::string_view &data);

template <>
bool Encoder::encode(const std::string &data);

}  // namespace Hessian2
