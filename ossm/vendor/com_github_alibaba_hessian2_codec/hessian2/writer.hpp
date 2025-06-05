#pragma once

#include "absl/strings/string_view.h"
#include "byte_order.h"

namespace Hessian2 {

class Writer {
 public:
  Writer() = default;
  virtual ~Writer() = default;
  virtual void rawWrite(const void* data, uint64_t size) = 0;
  virtual void rawWrite(absl::string_view data) = 0;

  void writeByte(uint8_t value) { rawWrite(std::addressof(value), 1); }

  template <typename T>
  void writeByte(T value) {
    writeByte(static_cast<uint8_t>(value));
  }

  template <ByteOrderType Endianness = ByteOrderType::Host, typename T>
  void write(
      typename std::enable_if<std::is_integral<T>::value, T>::type value) {
    const auto data = toEndian<Endianness>(value);
    rawWrite(reinterpret_cast<const char*>(std::addressof(data)), sizeof(T));
  }

  template <typename T>
  void writeLE(T value) {
    write<ByteOrderType::LittleEndian, T>(value);
  }

  template <typename T>
  void writeBE(T value) {
    write<ByteOrderType::BigEndian, T>(value);
  }
};

using WriterPtr = std::unique_ptr<Writer>;

}  // namespace Hessian2
