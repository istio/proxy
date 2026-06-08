#include "gtest/gtest.h"
#include "hessian2/test_framework/process.h"

namespace Hessian2 {

TEST(ProcessTest, echo) {
  Process pro;
  EXPECT_TRUE(pro.Run("bash -c 'echo -n 3'"));
  EXPECT_EQ(pro.Output(), "3");
}

TEST(ProcessTest, pipe) {
  Process pro;
  EXPECT_TRUE(pro.Run("bash -c 'ls /tmp | ls -ld'"));
}

TEST(ProcessTest, PipeWriteMode) {
  Process pro;
  EXPECT_TRUE(pro.RunWithWriteMode("read"));
  EXPECT_TRUE(pro.write(std::string("test")));
}

TEST(ProcessTest, Binary) {
  Process pro;
  EXPECT_TRUE(
      pro.Run("bash -c 'java -jar test_hessian/target/test_hessian-1.0.0.jar "
              "replyBinary_0'"));
  std::string expect;
  expect.push_back(0x20);
  EXPECT_EQ(pro.Output(), expect);
}
}  // namespace Hessian2
