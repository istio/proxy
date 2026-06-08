#pragma once

#include <iomanip>
#include <sstream>
#include <string>

namespace Hessian2 {
class Utils {
 public:
  static char* allocStringBuffer(std::string* str, size_t length) {
    str->reserve(length);
    str->resize(length - 1);
    return &((*str)[0]);
  }

  static std::string stringToHex(const std::string& in) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; in.length() > i; ++i) {
      ss << std::setw(2)
         << static_cast<unsigned int>(static_cast<unsigned char>(in[i]));
    }
    return ss.str();
  }

  template <class T>
  static void hashCombine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  void rawHashCombine(std::size_t& seed, std::size_t v) {
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

template <typename T>
struct static_const {
  static constexpr T value{};
};

template <typename T>
constexpr T static_const<T>::value;
}  // namespace Hessian2
