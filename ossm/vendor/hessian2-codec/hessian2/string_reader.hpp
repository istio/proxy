#pragma once

#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "hessian2/reader.hpp"

namespace Hessian2 {

class StringReader : public Reader {
 public:
  StringReader(absl::string_view data) : buffer_(data){};
  virtual ~StringReader() = default;

  virtual void rawReadNBytes(void* out, size_t len,
                             size_t peek_offset) override {
    ABSL_ASSERT(byteAvailable() + peek_offset >= len);
    absl::string_view data = buffer_.substr(offset() + peek_offset, len);
    uint8_t* dest = static_cast<uint8_t*>(out);
    memcpy(dest, data.data(), len);
  }
  virtual uint64_t length() const override { return buffer_.size(); }

 private:
  absl::string_view buffer_;
};

}  // namespace Hessian2
