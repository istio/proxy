#include "hessian2/basic_codec/ref_object_codec.hpp"

namespace Hessian2 {

// ref ::= x51 int
template <>
std::unique_ptr<RefObject> Decoder::decode() {
  auto ret = reader_->read<uint8_t>();
  ABSL_ASSERT(ret.first);
  ABSL_ASSERT(ret.second == 0x51);
  auto ref = decode<int32_t>();
  if (!ref) {
    return nullptr;
  }
  if (static_cast<uint32_t>(*ref) >= values_ref_.size()) {
    return nullptr;
  }
  return std::make_unique<RefObject>(values_ref_[*ref]);
}

template <>
bool Encoder::encode(const RefObject& value) {
  auto d = value.toRefDest();
  ABSL_ASSERT(d.has_value());
  auto ref = getValueRef(d.value());
  ABSL_ASSERT(ref != -1);
  writer_->writeByte<uint8_t>(0x51);
  encode<int32_t>(ref);
  return true;
}

}  // namespace Hessian2
