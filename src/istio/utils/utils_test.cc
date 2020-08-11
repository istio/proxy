/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/istio/utils/utils.h"

#include "gtest/gtest.h"

namespace istio {
namespace utils {
namespace {

class UtilsTest : public ::testing::Test {
 protected:
  void checkFalse(const std::string& principal) {
    std::string output_ns = "none";
    EXPECT_FALSE(GetSourceNamespace(principal, &output_ns));
    EXPECT_EQ(output_ns, output_ns);
  }

  void checkTrue(const std::string& principal, const std::string& ns) {
    std::string output_ns = "none";
    EXPECT_TRUE(GetSourceNamespace(principal, &output_ns));
    EXPECT_EQ(ns, output_ns);
  }
};

TEST_F(UtilsTest, TestGetSourceNamespace) {
  checkFalse("");
  checkFalse("cluster.local");
  checkFalse("cluster.local/");
  checkFalse("cluster.local/ns");
  checkFalse("cluster.local/sa/user");
  checkFalse("cluster.local/sa/user/ns");
  checkFalse("cluster.local/sa/user_ns/");
  checkFalse("cluster.local/sa/user_ns/abc/xyz");
  checkFalse("cluster.local/NS/abc");

  checkTrue("cluster.local/ns/", "");
  checkTrue("cluster.local/ns//", "");
  checkTrue("cluster.local/sa/user/ns/", "");
  checkTrue("cluster.local/ns//sa/user", "");
  checkTrue("cluster.local/ns//ns/ns", "");

  checkTrue("cluster.local/ns/ns/ns/ns", "ns");
  checkTrue("cluster.local/ns/abc_ns", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/sa/user_ns", "abc_ns");
  checkTrue("cluster.local/ns/abc_ns/sa/user_ns/other/xyz", "abc_ns");
  checkTrue("cluster.local/sa/user_ns/ns/abc", "abc");
  checkTrue("cluster.local/sa/user_ns/ns/abc/", "abc");
  checkTrue("cluster.local/sa/user_ns/ns/abc_ns", "abc_ns");
  checkTrue("cluster.local/sa/user_ns/ns/abc_ns/", "abc_ns");
}

}  // namespace
}  // namespace utils
}  // namespace istio
