#include "hessian2/reader.hpp"

namespace Hessian2 {

template <>
std::pair<bool, uint8_t> Reader::peek<uint8_t>(uint64_t peek_offset) {
  uint8_t result = 0;
  if (byteAvailable() - peek_offset < 1) {
    return {false, 0};
  }
  rawReadNBytes(&result, 1, peek_offset);
  return {true, result};
}

}  // namespace Hessian2
