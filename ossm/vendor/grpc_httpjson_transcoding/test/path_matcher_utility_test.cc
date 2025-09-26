// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/path_matcher_utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using google::api::HttpRule;
using google::grpc::transcoding::PathMatcherBuilder;
using google::grpc::transcoding::PathMatcherUtility;

using testing::_;
using testing::Eq;
using testing::Return;

class TestMethod {};

namespace google {
namespace grpc {
namespace transcoding {
template <>
class PathMatcherBuilder<const TestMethod*> {
 public:
  MOCK_METHOD5(Register,
               bool(const std::string&, const std::string&, const std::string&,
                    const std::unordered_set<std::string>&, const TestMethod*));
};
}  // namespace transcoding
}  // namespace grpc
}  // namespace google

class PathMatcherUtilityTest : public ::testing::Test {
 public:
  PathMatcherUtilityTest() : method1_(), method2_() {}

  PathMatcherBuilder<const TestMethod*> pmb;
  const TestMethod method1_;
  const TestMethod method2_;
};

TEST_F(PathMatcherUtilityTest, NeverRegister) {
  HttpRule http_rule;
  EXPECT_CALL(pmb, Register(_, _, _, _, _)).Times(0);
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
}

TEST_F(PathMatcherUtilityTest, RegisterGet) {
  HttpRule http_rule;
  http_rule.set_get("/path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("GET"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("GET"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterPut) {
  HttpRule http_rule;
  http_rule.set_put("/path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("PUT"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("PUT"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterPost) {
  HttpRule http_rule;
  http_rule.set_post("/path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("POST"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("POST"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterDelete) {
  HttpRule http_rule;
  http_rule.set_delete_("/path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("DELETE"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("DELETE"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterPatch) {
  HttpRule http_rule;
  http_rule.set_patch("/path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("PATCH"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("PATCH"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterCustom) {
  HttpRule http_rule;
  http_rule.mutable_custom()->set_kind("OPTIONS");
  http_rule.mutable_custom()->set_path("/custom_path");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("OPTIONS"), Eq("/custom_path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("OPTIONS"), Eq("/custom_path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterAdditionalBindings) {
  HttpRule http_rule;
  http_rule.set_get("/path");
  http_rule.set_body("body");

  HttpRule& custom_http_rule1 = *http_rule.add_additional_bindings();
  custom_http_rule1.mutable_custom()->set_kind("OPTIONS");
  custom_http_rule1.mutable_custom()->set_path("/custom_path");
  custom_http_rule1.set_body("body1");

  HttpRule& custom_http_rule2 = *http_rule.add_additional_bindings();
  custom_http_rule2.mutable_custom()->set_kind("HEAD");
  custom_http_rule2.mutable_custom()->set_path("/path");

  HttpRule& custom_http_rule3 = *http_rule.add_additional_bindings();
  custom_http_rule3.set_put("/put_path");

  EXPECT_CALL(pmb,
              Register(Eq("GET"), Eq("/path"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  EXPECT_CALL(pmb,
              Register(Eq("OPTIONS"), Eq("/custom_path"), Eq("body1"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  EXPECT_CALL(pmb,
              Register(Eq("HEAD"), Eq("/path"), Eq(""),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  EXPECT_CALL(pmb,
              Register(Eq("PUT"), Eq("/put_path"), Eq(""),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));

  EXPECT_CALL(
      pmb, Register(Eq("GET"), Eq("/path"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(true));
  EXPECT_CALL(
      pmb, Register(Eq("OPTIONS"), Eq("/custom_path"), Eq("body1"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}

TEST_F(PathMatcherUtilityTest, RegisterRootPath) {
  HttpRule http_rule;
  http_rule.set_get("/");
  http_rule.set_body("body");
  EXPECT_CALL(pmb,
              Register(Eq("GET"), Eq("/"), Eq("body"),
                       Eq(std::unordered_set<std::string>()), Eq(&method1_)))
      .WillOnce(Return(true));
  ASSERT_TRUE(
      PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, &method1_));
  EXPECT_CALL(
      pmb, Register(Eq("GET"), Eq("/"), Eq("body"),
                    Eq(std::unordered_set<std::string>{"key"}), Eq(&method2_)))
      .WillOnce(Return(false));
  ASSERT_FALSE(PathMatcherUtility::RegisterByHttpRule(pmb, http_rule, {"key"},
                                                      &method2_));
}
