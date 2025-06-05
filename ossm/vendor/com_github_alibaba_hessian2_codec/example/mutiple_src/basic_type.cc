#include "example/mutiple_src/basic_type.h"

#include <iostream>

#include "hessian2/codec.hpp"
#include "hessian2/basic_codec/object_codec.hpp"

int testBasicType() {
  {
    std::string out;
    ::Hessian2::Encoder encode(out);
    encode.encode<std::string>("test string");
    ::Hessian2::Decoder decode(out);
    auto ret = decode.decode<std::string>();
    if (ret) {
      std::cout << *ret << std::endl;
    } else {
      std::cerr << "decode failed: " << decode.getErrorMessage() << std::endl;
    }
  }
  {
    std::string out;
    ::Hessian2::Encoder encode(out);
    encode.encode<int64_t>(100);
    ::Hessian2::Decoder decode(out);
    auto ret = decode.decode<int64_t>();
    if (ret) {
      std::cout << *ret << std::endl;
    } else {
      std::cerr << "decode failed: " << decode.getErrorMessage() << std::endl;
    }
  }

  return 0;
}
