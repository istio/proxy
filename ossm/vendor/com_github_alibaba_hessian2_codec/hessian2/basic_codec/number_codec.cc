#include "hessian2/basic_codec/number_codec.hpp"

namespace Hessian2 {

namespace {

void writeBEDouble(WriterPtr &writer, const double &value) {
  uint64_t out;
  std::memcpy(&out, &value, 8);
  writer->writeBE<uint64_t>(out);
}

}  // namespace

template <>
std::unique_ptr<double> Decoder::decode() {
  auto out = std::make_unique<double>();
  uint8_t code = reader_->readBE<uint8_t>().second;
  switch (code) {
    // ::= x5b                   # 0.0
    case 0x5b:
      *out.get() = 0.0;
      return out;
    // ::= x5c                   # 1.0
    case 0x5c:
      *out.get() = 1.0;
      return out;
    // ::= x5d b0                # byte cast to double (-128.0 to 127.0)
    case 0x5d:
      if (reader_->byteAvailable() < 1) {
        return nullptr;
      }
      *out.get() = static_cast<double>(reader_->readBE<int8_t>().second);
      return out;
    // ::= x5e b1 b0             # short cast to double
    case 0x5e:
      if (reader_->byteAvailable() < 2) {
        return nullptr;
      }
      *out.get() = static_cast<double>(reader_->readBE<int16_t>().second);
      return out;
    // ::= x5f b3 b2 b1 b0       # 32-bit float cast to double
    case 0x5f:
      if (reader_->byteAvailable() < 4) {
        return nullptr;
      }
      *out.get() = readBE<double, 4>(reader_);
      return out;
    // ::= 'D' b7 b6 b5 b4 b3 b2 b1 b0
    case 'D':
      if (reader_->byteAvailable() < 8) {
        return nullptr;
      }
      *out.get() = readBE<double, 8>(reader_);
      return out;
  }
  return nullptr;
}

template <>
bool Encoder::encode(const double &value) {
  int32_t int_value = static_cast<int32_t>(value);
  if (int_value == value) {
    if (int_value == 0) {
      writer_->writeByte(0x5b);
      return true;
    }

    if (int_value == 1) {
      writer_->writeByte(0x5c);
      return true;
    }

    if (int_value >= -0x80 && int_value < 0x80) {
      writer_->writeByte(0x5d);
      writer_->writeBE<int8_t>(int_value);
      return true;
    }

    if (int_value >= -0x8000 && int_value < 0x8000) {
      writer_->writeByte(0x5e);
      writer_->writeBE<int8_t>(int_value >> 8);
      writer_->writeBE<uint8_t>(int_value);
      return true;
    }
  }

  writer_->writeByte(0x44);
  writeBEDouble(writer_, value);
  return true;
}

// # 32-bit signed integer
// ::= 'I' b3 b2 b1 b0
// ::= [x80-xbf]             # -x10 to x3f
// ::= [xc0-xcf] b0          # -x800 to x7ff
// ::= [xd0-xd7] b1 b0       # -x40000 to x3ffff
template <>
std::unique_ptr<int32_t> Decoder::decode() {
  auto out = std::make_unique<int32_t>();
  uint8_t code = reader_->readBE<uint8_t>().second;

  // ::= [x80-xbf]             # -x10 to x3f
  if (code >= 0x80 && code <= 0xbf) {
    *out.get() = (code - 0x90);
    return out;
  }
  switch (code) {
    // ::= [xc0-xcf] b0          # -x800 to x7ff
    case 0xc0:
    case 0xc1:
    case 0xc2:
    case 0xc3:
    case 0xc4:
    case 0xc5:
    case 0xc6:
    case 0xc7:
    case 0xc8:
    case 0xc9:
    case 0xca:
    case 0xcb:
    case 0xcc:
    case 0xcd:
    case 0xce:
    case 0xcf:
      if (reader_->byteAvailable() < 1) {
        return nullptr;
      }
      *out.get() = LeftShift<int16_t>(code - 0xc8, 8) +
                   reader_->readBE<uint8_t>().second;
      return out;
    // ::= [xd0-xd7] b1 b0       # -x40000 to x3ffff
    case 0xd0:
    case 0xd1:
    case 0xd2:
    case 0xd3:
    case 0xd4:
    case 0xd5:
    case 0xd6:
    case 0xd7:
      if (reader_->byteAvailable() < 2) {
        return nullptr;
      }
      *out.get() = LeftShift<int32_t>(code - 0xd4, 16) +
                   reader_->readBE<uint16_t>().second;
      return out;
    // ::= 'I' b3 b2 b1 b0
    case 0x49:
      if (reader_->byteAvailable() < 4) {
        return nullptr;
      }
      *out.get() = reader_->readBE<int32_t>().second;
      return out;
  }

  return nullptr;
}

// # 32-bit signed integer
// ::= 'I' b3 b2 b1 b0
// ::= [x80-xbf]             # -x10 to x3f
// ::= [xc0-xcf] b0          # -x800 to x7ff
// ::= [xd0-xd7] b1 b0       # -x40000 to x3ffff
template <>
bool Encoder::encode(const int32_t &data) {
  if (data >= -0x10 && data <= 0x2f) {
    writer_->writeByte(data + 0x90);
    return true;
  }

  if (data >= -0x800 && data <= 0x7ff) {
    writer_->writeByte(0xc8 + (data >> 8));
    writer_->writeByte(data);
    return true;
  }

  if (data >= -0x40000 && data <= 0x3ffff) {
    writer_->writeByte(0xd4 + (data >> 16));
    writer_->writeByte(data >> 8);
    writer_->writeByte(data);
    return true;
  }
  writer_->writeByte(0x49);
  writer_->writeBE<uint32_t>(data);
  return true;
}

// # 64-bit signed long integer
// ::= 'L' b7 b6 b5 b4 b3 b2 b1 b0
// ::= [xd8-xef]             # -x08 to x0f
// ::= [xf0-xff] b0          # -x800 to x7ff
// ::= [x38-x3f] b1 b0       # -x40000 to x3ffff
// ::= x59 b3 b2 b1 b0       # 32-bit integer cast to long
template <>
std::unique_ptr<int64_t> Decoder::decode() {
  auto out = std::make_unique<int64_t>();
  uint8_t code = reader_->readBE<uint8_t>().second;
  switch (code) {
    // ::= [xd8-xef]             # -x08 to x0f
    case 0xd8:
    case 0xd9:
    case 0xda:
    case 0xdb:
    case 0xdc:
    case 0xdd:
    case 0xde:
    case 0xdf:
    case 0xe0:
    case 0xe1:
    case 0xe2:
    case 0xe3:
    case 0xe4:
    case 0xe5:
    case 0xe6:
    case 0xe7:
    case 0xe8:
    case 0xe9:
    case 0xea:
    case 0xeb:
    case 0xec:
    case 0xed:
    case 0xee:
    case 0xef:
      *out.get() = (code - 0xe0);
      return out;
    // ::= [xf0-xff] b0          # -x800 to x7ff
    case 0xf0:
    case 0xf1:
    case 0xf2:
    case 0xf3:
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7:
    case 0xf8:
    case 0xf9:
    case 0xfa:
    case 0xfb:
    case 0xfc:
    case 0xfd:
    case 0xfe:
    case 0xff:
      if (reader_->byteAvailable() < 1) {
        return nullptr;
      }
      *out.get() = LeftShift<int16_t>(code - 0xf8, 8) +
                   reader_->readBE<uint8_t>().second;
      return out;
    // ::= [x38-x3f] b1 b0       # -x40000 to x3ffff
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
      if (reader_->byteAvailable() < 2) {
        return nullptr;
      }
      *out.get() = LeftShift<int32_t>(code - 0x3c, 16) +
                   reader_->readBE<uint16_t>().second;
      return out;
    // ::= x59 b3 b2 b1 b0       # 32-bit integer cast to long
    case 0x59:
      if (reader_->byteAvailable() < 4) {
        return nullptr;
      }
      *out.get() = reader_->readBE<int32_t>().second;
      return out;
    // ::= 'L' b7 b6 b5 b4 b3 b2 b1 b0
    case 0x4c:
      if (reader_->byteAvailable() < 8) {
        return nullptr;
      }
      *out.get() = reader_->readBE<int64_t>().second;
      return out;
  }
  return nullptr;
}

// # 64-bit signed long integer
// ::= 'L' b7 b6 b5 b4 b3 b2 b1 b0
// ::= [xd8-xef]             # -x08 to x0f
// ::= [xf0-xff] b0          # -x800 to x7ff
// ::= [x38-x3f] b1 b0       # -x40000 to x3ffff
// ::= x59 b3 b2 b1 b0       # 32-bit integer cast to long
template <>
bool Encoder::encode(const int64_t &data) {
  if (data >= -0x08 && data <= 0x0f) {
    writer_->writeByte(data + 0xe0);
    return true;
  }

  if (data >= -0x800 && data <= 0x7ff) {
    writer_->writeByte(0xf8 + (data >> 8));
    writer_->writeByte(data);
    return true;
  }

  if (data >= -0x40000 && data <= 0x3ffff) {
    writer_->writeByte(0x3c + (data >> 16));
    writer_->writeByte(data >> 8);
    writer_->writeByte(data);
    return true;
  }

  if (data >= -0x80000000L && data <= 0x7fffffffL) {
    writer_->writeByte(0x59);
    writer_->writeBE<int32_t>(data);
    return true;
  }

  writer_->writeByte(0x4c);
  writer_->writeBE<int64_t>(data);
  return true;
}

template <>
bool Encoder::encode(const int8_t &data) {
  return encode<int32_t>(data);
}

template <>
bool Encoder::encode(const int16_t &data) {
  return encode<int32_t>(data);
}

template <>
bool Encoder::encode(const uint8_t &data) {
  return encode<int32_t>(data);
}

template <>
bool Encoder::encode(const uint16_t &data) {
  return encode<int32_t>(data);
}

template <>
bool Encoder::encode(const uint32_t &data) {
  return encode<int64_t>(data);
}

// Encoding and decoding of uint64_t is not supported because Java 64-bit
// integers are signed.

}  // namespace Hessian2
