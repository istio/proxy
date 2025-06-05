#include "hessian2/basic_codec/date_codec.hpp"

namespace Hessian2 {

namespace {

template <typename T>
std::unique_ptr<T> readDate(ReaderPtr &reader) {
  auto out = std::make_unique<T>();
  uint8_t code = reader->read<uint8_t>().second;
  switch (code) {
    case 0x4b:
      if (reader->byteAvailable() < 4) {
        return nullptr;
      }
      return std::make_unique<T>(std::chrono::duration_cast<T>(
          std::chrono::minutes(reader->readBE<int32_t>().second)));
    case 0x4a:
      if (reader->byteAvailable() < 8) {
        return nullptr;
      }
      return std::make_unique<T>(std::chrono::duration_cast<T>(
          std::chrono::milliseconds(reader->readBE<int64_t>().second)));
  }
  return nullptr;
}

template <typename T>
void writeDate(WriterPtr &writer, const T &value) {
  int64_t value_in_min =
      std::chrono::duration_cast<std::chrono::minutes>(value).count();
  int64_t value_in_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(value).count();
  if (value_in_min * 60000 == value_in_ms) {
    writer->writeByte(0x4b);
    writer->writeBE<int32_t>(value_in_min);
  } else {
    writer->writeByte(0x4a);
    writer->writeBE<int64_t>(value_in_ms);
  }
}

}  // namespace

// # time in UTC encoded as 64-bit long milliseconds since epoch
// ::= x4a b7 b6 b5 b4 b3 b2 b1 b0
// ::= x4b b3 b2 b1 b0       # minutes since epoch
template <>
std::unique_ptr<std::chrono::milliseconds> Decoder::decode() {
  return readDate<std::chrono::milliseconds>(reader_);
}

template <>
std::unique_ptr<std::chrono::minutes> Decoder::decode() {
  return readDate<std::chrono::minutes>(reader_);
}

template <>
std::unique_ptr<std::chrono::seconds> Decoder::decode() {
  return readDate<std::chrono::seconds>(reader_);
}

template <>
std::unique_ptr<std::chrono::hours> Decoder::decode() {
  return readDate<std::chrono::hours>(reader_);
}

#if defined(_LIBCPP_STD_VER) && _LIBCPP_STD_VER > 17
template <>
std::unique_ptr<std::chrono::days> Decoder::decode() {
  return readDate<std::chrono::days>(reader_);
}
template <>
std::unique_ptr<std::chrono::weeks> Decoder::decode() {
  return readDate<std::chrono::weeks>(reader_);
}
template <>
std::unique_ptr<std::chrono::years> Decoder::decode() {
  return readDate<std::chrono::years>(reader_);
}
template <>
std::unique_ptr<std::chrono::months> Decoder::decode() {
  return readDate<std::chrono::months>(reader_);
}
#endif

// # time in UTC encoded as 64-bit long milliseconds since epoch
// ::= x4a b7 b6 b5 b4 b3 b2 b1 b0
// ::= x4b b3 b2 b1 b0       # minutes since epoch

template <>
bool Encoder::encode(const std::chrono::minutes &value) {
  writer_->writeByte(0x4b);
  writer_->writeBE<int32_t>(value.count());
  return true;
}

template <>
bool Encoder::encode(const std::chrono::milliseconds &value) {
  std::chrono::minutes value_min =
      std::chrono::duration_cast<std::chrono::minutes>(value);
  if (value_min.count() * 60000 == value.count()) {
    return encode<std::chrono::minutes>(value_min);
  }
  writer_->writeByte(0x4a);
  writer_->writeBE<int64_t>(value.count());
  return true;
}

template <>
bool Encoder::encode(const std::chrono::seconds &value) {
  writeDate<std::chrono::seconds>(writer_, value);
  return true;
}

template <>
bool Encoder::encode(const std::chrono::hours &value) {
  writeDate<std::chrono::hours>(writer_, value);
  return true;
}

#if defined(_LIBCPP_STD_VER) && _LIBCPP_STD_VER > 17

template <>
bool Encoder::encode(const std::chrono::days &value) {
  writeDate<std::chrono::days>(writer_, value);
  return true;
}
template <>
bool Encoder::encode(const std::chrono::weeks &value) {
  writeDate<std::chrono::weeks>(writer_, value);
  return true;
}
template <>
bool Encoder::encode(const std::chrono::years &value) {
  writeDate<std::chrono::years>(writer_, value);
  return true;
}
template <>
bool Encoder::encode(const std::chrono::months &value) {
  writeDate<std::chrono::months>(writer_, value);
  return true;
}
#endif

}  // namespace Hessian2
