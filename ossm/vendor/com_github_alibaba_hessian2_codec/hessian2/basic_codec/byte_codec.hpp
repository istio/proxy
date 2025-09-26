#pragma once

#include <ctime>

#include "hessian2/codec.hpp"

/////////////////////////////////////////
// Binary, []byte
/////////////////////////////////////////

namespace Hessian2 {

bool decodeBytesWithReader(std::vector<uint8_t> &output, ReaderPtr &reader);
bool readBytes(std::vector<uint8_t> &output, ReaderPtr &reader, size_t length,
               bool is_last_chunk);

template <>
std::unique_ptr<std::vector<uint8_t>> Decoder::decode();

template <>
bool Encoder::encode(const std::vector<uint8_t> &data);

}  // namespace Hessian2
