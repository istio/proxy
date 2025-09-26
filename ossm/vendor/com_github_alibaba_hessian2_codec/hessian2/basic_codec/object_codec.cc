#include "hessian2/basic_codec/object_codec.hpp"

namespace Hessian2 {

template <>
std::unique_ptr<NullObject> Decoder::decode() {
  auto ret = reader_->read<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }

  ABSL_ASSERT(ret.second == 'N');
  return std::make_unique<NullObject>();
}

template <>
bool Encoder::encode(const NullObject&) {
  writer_->writeByte('N');
  return true;
}

/*
x00 - x1f    # utf-8 string length 0-32
x20 - x2f    # binary data length 0-16
x30 - x33    # utf-8 string length 0-1023
x34 - x37    # binary data length 0-1023
x38 - x3f    # three-octet compact long (-x40000 to x3ffff)
x40          # reserved (expansion/escape)
x41          # 8-bit binary data non-final chunk ('A')
x42          # 8-bit binary data final chunk ('B')
x43          # object type definition ('C')
x44          # 64-bit IEEE encoded double ('D')
x45          # reserved
x46          # boolean false ('F')
x47          # reserved
x48          # untyped map ('H')
x49          # 32-bit signed integer ('I')
x4a          # 64-bit UTC millisecond date
x4b          # 32-bit UTC minute date
x4c          # 64-bit signed long integer ('L')
x4d          # map with type ('M')
x4e          # null ('N')
x4f          # object instance ('O')
x50          # reserved
x51          # reference to map/list/object - integer ('Q')
x52          # utf-8 string non-final chunk ('R')
x53          # utf-8 string final chunk ('S')
x54          # boolean true ('T')
x55          # variable-length list/vector ('U')
x56          # fixed-length list/vector ('V')
x57          # variable-length untyped list/vector ('W')
x58          # fixed-length untyped list/vector ('X')
x59          # long encoded as 32-bit int ('Y')
x5a          # list/map terminator ('Z')
x5b          # double 0.0
x5c          # double 1.0
x5d          # double represented as byte (-128.0 to 127.0)
x5e          # double represented as short (-32768.0 to 327676.0)
x5f          # double represented as float
x60 - x6f    # object with direct type
x70 - x77    # fixed list with direct length
x78 - x7f    # fixed untyped list with direct length
x80 - xbf    # one-octet compact int (-x10 to x3f, x90 is 0)
xc0 - xcf    # two-octet compact int (-x800 to x7ff)
xd0 - xd7    # three-octet compact int (-x40000 to x3ffff)
xd8 - xef    # one-octet compact long (-x8 to xf, xe0 is 0)
xf0 - xff    # two-octet compact long (-x800 to x7ff, xf8 is 0)
*/

template <>
std::unique_ptr<Object> Decoder::decode() {
  auto ret = reader_->peek<uint8_t>();
  if (!ret.first) {
    return nullptr;
  }
  auto code = ret.second;

  switch (code) {
    // Null Object
    case 'N': {
      auto ret = decode<NullObject>();
      return ret == nullptr ? nullptr : std::make_unique<NullObject>();
    }
    // Bool
    case 0x46:
    case 0x54: {
      auto ret = decode<bool>();
      return ret == nullptr ? nullptr : std::make_unique<BooleanObject>(*ret);
    }

    // Date
    case 0x4a:
    case 0x4b: {
      auto ret = decode<std::chrono::milliseconds>();
      return ret == nullptr ? nullptr : std::make_unique<DateObject>(*ret);
    }

    // Double
    case 0x5b:
    case 0x5c:
    case 0x5d:
    case 0x5e:
    case 0x5f:
    case 'D': {
      auto ret = decode<double>();
      return ret == nullptr ? nullptr : std::make_unique<DoubleObject>(*ret);
    }

    // Typed list
    case 'V':
    case 0x55:
    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73:
    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77: {
      auto ret = decode<TypedListObject>();
      return ret;
    }

    // Untyped list
    case 0x57:
    case 0x58:
    case 0x78:
    case 0x79:
    case 0x7a:
    case 0x7b:
    case 0x7c:
    case 0x7d:
    case 0x7e:
    case 0x7f: {
      auto ret = decode<UntypedListObject>();
      return ret;
    }

    // Typed map
    case 'M': {
      auto ret = decode<TypedMapObject>();
      return ret;
    }

    case 'H': {
      auto ret = decode<UntypedMapObject>();
      return ret;
    }

    case 'C': {
      auto ret = decode<Object::Definition>();
      if (!ret) {
        return nullptr;
      }
      return decode<Object>();
    }

    case 'O':
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
    case 0x6e:
    case 0x6f: {
      auto ret = decode<ClassInstanceObject>();
      return ret;
    }

    case 0x51: {
      auto ret = decode<RefObject>();
      return ret;
    }

    default:
      break;
  }

  // String
  if (code <= 0x1f || (code >= 0x30 && code <= 0x33) || code == 0x52 ||
      code == 0x53) {
    auto ret = decode<std::string>();
    return ret == nullptr ? nullptr
                          : std::make_unique<StringObject>(std::move(ret));
  }

  // Binary
  if ((code >= 0x20 && code <= 0x2f) || (code >= 0x34 && code <= 0x37) ||
      code == 0x41 || code == 0x42) {
    auto ret = decode<std::vector<uint8_t>>();
    return ret == nullptr ? nullptr
                          : std::make_unique<BinaryObject>(std::move(ret));
  }

  // Long
  if ((code >= 0x38 && code <= 0x3f) || (code >= 0xd8 && code <= 0xef) ||
      code >= 0xf0 || code == 0x59 || code == 0x4c) {
    auto ret = decode<int64_t>();
    return ret == nullptr ? nullptr : std::make_unique<LongObject>(*ret);
  }

  // int
  if (code == 0x49 || (code >= 0x80 && code <= 0xbf) ||
      (code >= 0xc0 && code <= 0xcf) || (code >= 0xd0 && code <= 0xd7)) {
    auto ret = decode<int32_t>();
    return ret == nullptr ? nullptr : std::make_unique<IntegerObject>(*ret);
  }

  return nullptr;
}

template <>
bool Encoder::encode(const Object& value) {
  switch (value.type()) {
    case Object::Type::Binary: {
      return encode<std::vector<uint8_t>>(value.toBinary().value().get());
    }
    case Object::Type::Boolean: {
      return encode<bool>(value.toBoolean().value().get());
    }
    case Object::Type::Date: {
      return encode<std::chrono::milliseconds>(value.toDate().value().get());
    }
    case Object::Type::Double: {
      return encode<double>(value.toDouble().value().get());
    }
    case Object::Type::Integer: {
      return encode<int32_t>(value.toInteger().value().get());
    }
    case Object::Type::Long: {
      return encode<int64_t>(value.toLong().value().get());
    }
    case Object::Type::Null: {
      NullObject o;
      return encode<NullObject>(o);
    }
    case Object::Type::Ref: {
      return encode<RefObject>(*dynamic_cast<const RefObject*>(&value));
    }
    case Object::Type::String: {
      return encode<std::string>(value.toString().value().get());
    }
    case Object::Type::TypedList: {
      return encode<TypedListObject>(
          *dynamic_cast<const TypedListObject*>(&value));
    }
    case Object::Type::UntypedList: {
      return encode<UntypedListObject>(
          *dynamic_cast<const UntypedListObject*>(&value));
    }
    case Object::Type::TypedMap: {
      return encode<TypedMapObject>(
          *dynamic_cast<const TypedMapObject*>(&value));
    }
    case Object::Type::UntypedMap: {
      return encode<UntypedMapObject>(
          *dynamic_cast<const UntypedMapObject*>(&value));
    }
    case Object::Type::Class: {
      return encode<ClassInstanceObject>(
          *dynamic_cast<const ClassInstanceObject*>(&value));
    }
    default:
      return false;
  }
}
}  // namespace Hessian2
