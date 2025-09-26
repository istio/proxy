#include "hessian2/basic_codec/byte_codec.hpp"

namespace Hessian2 {

namespace {
constexpr size_t CHUNK_SIZE = 1024;
}

// # 8-bit binary data split into 64k chunks
// ::= x41(A) b1 b0 <binary-data> binary # non-final chunk
// ::= x42(B) b1 b0 <binary-data>        # final chunk
// ::= [x20-x2f] <binary-data>           # binary data of length 0-15
// ::= [x34-x37] <binary-data>           # binary data of length 0-1023
template <>
std::unique_ptr<std::vector<uint8_t>> Decoder::decode() {
  auto out = std::make_unique<std::vector<uint8_t>>();
  if (!decodeBytesWithReader(*out.get(), reader_)) {
    return nullptr;
  }
  return out;
}

bool decodeBytesWithReader(std::vector<uint8_t> &output, ReaderPtr &reader) {
  uint8_t code = reader->read<uint8_t>().second;
  switch (code) {
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f:
      if (!readBytes(output, reader, code - 0x20, true)) {
        return false;
      }
      return true;
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37: {
      auto res = reader->read<uint8_t>();
      if (!res.first ||
          !readBytes(output, reader, ((code - 0x34) << 8) + res.second, true)) {
        return false;
      }
      return true;
    }
    case 0x42: {
      auto res = reader->readBE<uint16_t>();
      if (!res.first) {
        return false;
      }
      return readBytes(output, reader, res.second, true);
    }
    case 0x41: {
      auto res = reader->readBE<uint16_t>();
      if (!res.first) {
        return false;
      }
      return readBytes(output, reader, res.second, false);
    }
  }
  return false;
}

// # 8-bit binary data split into 64k chunks
// ::= x41('A') b1 b0 <binary-data> binary # non-final chunk
// ::= x42('B') b1 b0 <binary-data>        # final chunk
// ::= [x20-x2f] <binary-data>  # binary data of length 0-15
// ::= [x34-x37] <binary-data>  # binary data of length 0-1023
template <>
bool Encoder::encode(const std::vector<uint8_t> &data) {
  size_t size = data.size();
  if (size < 16) {
    writer_->writeByte(0x20 + size);
    writer_->rawWrite(
        absl::string_view(std::move(std::string(data.begin(), data.end()))));
    return true;
  }
  if (size < 1024) {
    writer_->writeByte(0x34 + (size >> 8));
    writer_->writeByte(size);
    writer_->rawWrite(
        absl::string_view(std::move(std::string(data.begin(), data.end()))));
    return true;
  }

  uint32_t offset = 0;
  while (size > CHUNK_SIZE) {
    writer_->writeByte(0x41);
    writer_->writeBE<uint16_t>(CHUNK_SIZE);
    size -= CHUNK_SIZE;
    writer_->rawWrite(absl::string_view(std::move(std::string(
        data.begin() + offset, data.begin() + offset + CHUNK_SIZE))));
    offset += CHUNK_SIZE;
  }

  if (size > 0) {
    writer_->writeByte(0x42);
    writer_->writeBE<uint16_t>(size);
    writer_->rawWrite(absl::string_view(
        std::move(std::string(data.begin() + offset, data.end()))));
  }
  return true;
}

bool readBytes(std::vector<uint8_t> &output, ReaderPtr &reader, size_t length,
               bool is_last_chunk) {
  if (length == 0) {
    return true;
  }
  if (length > reader->byteAvailable()) {
    return false;
  }
  auto offset = output.size();
  output.resize(offset + length);
  reader->readNBytes(&output[offset], length);
  if (is_last_chunk) {
    return true;
  }
  return decodeBytesWithReader(output, reader);
}

}  // namespace Hessian2
