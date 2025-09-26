#pragma once

#include <cstdio>
#include <fstream>

#include "absl/strings/str_format.h"
#include "gtest/gtest.h"
#include "hessian2/codec.hpp"
#include "hessian2/test_framework/process.h"
#include "hessian2/test_framework/test_common.h"

namespace {
std::string GenerateTestCaseFullName(const std::string& testcase_name,
                                     const std::string& err_tmp_file,
                                     const std::string& out_tmp_file) {
  return absl::StrFormat(
      "bash -c 'java -jar test_hessian/target/test_hessian-1.0.0.jar %s 2> %s "
      "1> %s'",
      testcase_name, err_tmp_file, out_tmp_file);
}
}  // namespace

namespace Hessian2 {
class TestEncoderFramework : public testing::Test {
 public:
  template <typename T>
  bool Encode(const std::string& testcase_name, const T& input) {
    Process pro;
    test_common::TmpFile err_tmp_file;
    test_common::TmpFile out_tmp_file;
    if (!pro.RunWithWriteMode(GenerateTestCaseFullName(
            testcase_name, err_tmp_file.GetTmpfileName(),
            out_tmp_file.GetTmpfileName()))) {
      return false;
    }
    std::string out;
    Hessian2::Encoder encoder(out);
    encoder.encode<T>(input);
    EXPECT_TRUE(pro.write(out));
    auto err = err_tmp_file.GetFileContent();
    EXPECT_EQ(err, std::string());

    auto output = out_tmp_file.GetFileContent();
    EXPECT_EQ(output, std::string("true"));
    return true;
  }
};

}  // namespace Hessian2
