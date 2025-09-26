#include "gtest/gtest.h"
#include "helper.h"
#include "example/proto/routeguide.grpc.pb.h"

using routeguide::Feature;

class FeatureTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

};

TEST_F(FeatureTest, testName) {
  Feature f;
  f.set_name("foo");
  EXPECT_EQ("foo", f.name());
}

int main(int ac, char* av[]) {
  testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}
