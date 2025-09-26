#pragma once

#include <ctime>

#include "hessian2/codec.hpp"

/////////////////////////////////////////
// Boolean
/////////////////////////////////////////

namespace Hessian2 {

template <>
std::unique_ptr<bool> Decoder::decode();

template <>
bool Encoder::encode(const bool &value);

}  // namespace Hessian2
