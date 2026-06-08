#pragma once

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "hessian2/codec.hpp"
#include "hessian2/test_framework/process.h"

namespace {
std::string GenerateTestCaseFullName(const std::string& testcase_name) {
  return absl::StrFormat(
      "bash -c 'java -jar test_hessian/target/test_hessian-1.0.0.jar %s'",
      testcase_name);
}
}  // namespace

namespace Hessian2 {

class TestDecoderFramework : public testing::Test {
 public:
  template <typename T>
  bool Decode(const std::string& testcase_name, const T& expect_output,
              bool ignore_equal = false) {
    Process pro;
    if (!pro.Run(GenerateTestCaseFullName(testcase_name))) {
      return false;
    }
    auto output = pro.Output();
    Hessian2::Decoder decoder(output);
    auto decode_output = decoder.decode<T>();
    // For RefObject, we only need to compare whether the referenced object
    // pointer is the same, not whether the actual object content of the
    // comparator is the same. Therefore, the decode RefObject  and the
    // comparison RefObject must be different, so the RefObject comparison can
    // be ignored here.
    if (!ignore_equal) {
      EXPECT_EQ(*decode_output, expect_output);
    }
    EXPECT_EQ(output.size(), decoder.offset());
    return true;
  }
};

}  // namespace Hessian2
