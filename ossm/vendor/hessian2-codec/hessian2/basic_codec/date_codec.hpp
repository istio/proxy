#pragma once

#include "hessian2/codec.hpp"

/////////////////////////////////////////
// Bool
/////////////////////////////////////////

namespace Hessian2 {

template <>
std::unique_ptr<std::chrono::milliseconds> Decoder::decode();

template <>
std::unique_ptr<std::chrono::minutes> Decoder::decode();

template <>
std::unique_ptr<std::chrono::seconds> Decoder::decode();

template <>
std::unique_ptr<std::chrono::hours> Decoder::decode();

#if defined(_LIBCPP_STD_VER) && _LIBCPP_STD_VER > 17

template <>
std::unique_ptr<std::chrono::days> Decoder::decode();

template <>
std::unique_ptr<std::chrono::weeks> Decoder::decode();

template <>
std::unique_ptr<std::chrono::years> Decoder::decode();

template <>
std::unique_ptr<std::chrono::months> Decoder::decode();
#endif

template <>
bool Encoder::encode(const std::chrono::minutes &value);

template <>
bool Encoder::encode(const std::chrono::milliseconds &value);

template <>
bool Encoder::encode(const std::chrono::seconds &value);

template <>
bool Encoder::encode(const std::chrono::hours &value);

#if defined(_LIBCPP_STD_VER) && _LIBCPP_STD_VER > 17

template <>
bool Encoder::encode(const std::chrono::days &value);

template <>
bool Encoder::encode(const std::chrono::weeks &value);

template <>
bool Encoder::encode(const std::chrono::years &value);

template <>
bool Encoder::encode(const std::chrono::months &value);
#endif

}  // namespace Hessian2
