// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/compiler/resolver.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "common/value.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/cel_value.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::IntValue;
using ::cel::TypeValue;
using ::testing::Eq;

class FakeFunction : public CelFunction {
 public:
  explicit FakeFunction(const std::string& name)
      : CelFunction(CelFunctionDescriptor{name, false, {}}) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    return absl::OkStatus();
  }
};

class ResolverTest : public testing::Test {
 public:
  ResolverTest() = default;

 protected:
  CelTypeRegistry type_registry_;
};

TEST_F(ResolverTest, TestFullyQualifiedNames) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto names = resolver.FullyQualifiedNames("simple_name");
  std::vector<std::string> expected_names(
      {"google.api.expr.simple_name", "google.api.simple_name",
       "google.simple_name", "simple_name"});
  EXPECT_THAT(names, Eq(expected_names));
}

TEST_F(ResolverTest, TestFullyQualifiedNamesPartiallyQualifiedName) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto names = resolver.FullyQualifiedNames("expr.simple_name");
  std::vector<std::string> expected_names(
      {"google.api.expr.expr.simple_name", "google.api.expr.simple_name",
       "google.expr.simple_name", "expr.simple_name"});
  EXPECT_THAT(names, Eq(expected_names));
}

TEST_F(ResolverTest, TestFullyQualifiedNamesAbsoluteName) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto names = resolver.FullyQualifiedNames(".google.api.expr.absolute_name");
  EXPECT_THAT(names.size(), Eq(1));
  EXPECT_THAT(names[0], Eq("google.api.expr.absolute_name"));
}

TEST_F(ResolverTest, TestFindConstantEnum) {
  CelFunctionRegistry func_registry;
  type_registry_.Register(TestMessage::TestEnum_descriptor());

  Resolver resolver("google.api.expr.runtime.TestMessage",
                    func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto enum_value = resolver.FindConstant("TestEnum.TEST_ENUM_1", -1);
  ASSERT_TRUE(enum_value);
  ASSERT_TRUE(enum_value->Is<IntValue>());
  EXPECT_THAT(enum_value->GetInt().NativeValue(), Eq(1L));

  enum_value = resolver.FindConstant(
      ".google.api.expr.runtime.TestMessage.TestEnum.TEST_ENUM_2", -1);
  ASSERT_TRUE(enum_value);
  ASSERT_TRUE(enum_value->Is<IntValue>());
  EXPECT_THAT(enum_value->GetInt().NativeValue(), Eq(2L));
}

TEST_F(ResolverTest, TestFindConstantUnqualifiedType) {
  CelFunctionRegistry func_registry;
  Resolver resolver("cel", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto type_value = resolver.FindConstant("int", -1);
  EXPECT_TRUE(type_value);
  EXPECT_TRUE(type_value->Is<TypeValue>());
  EXPECT_THAT(type_value->GetType().name(), Eq("int"));
}

TEST_F(ResolverTest, TestFindConstantFullyQualifiedType) {
  google::protobuf::LinkMessageReflection<TestMessage>();
  CelFunctionRegistry func_registry;
  Resolver resolver("cel", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto type_value =
      resolver.FindConstant(".google.api.expr.runtime.TestMessage", -1);
  ASSERT_TRUE(type_value);
  ASSERT_TRUE(type_value->Is<TypeValue>());
  EXPECT_THAT(type_value->GetType().name(),
              Eq("google.api.expr.runtime.TestMessage"));
}

TEST_F(ResolverTest, TestFindConstantQualifiedTypeDisabled) {
  CelFunctionRegistry func_registry;
  Resolver resolver("", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider(), false);
  auto type_value =
      resolver.FindConstant(".google.api.expr.runtime.TestMessage", -1);
  EXPECT_FALSE(type_value);
}

TEST_F(ResolverTest, FindTypeBySimpleName) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr.runtime",
                    func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  ASSERT_OK_AND_ASSIGN(auto type, resolver.FindType("TestMessage", -1));
  EXPECT_TRUE(type.has_value());
  EXPECT_EQ(type->second.name(), "google.api.expr.runtime.TestMessage");
}

TEST_F(ResolverTest, FindTypeByQualifiedName) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr.runtime",
                    func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  ASSERT_OK_AND_ASSIGN(
      auto type, resolver.FindType(".google.api.expr.runtime.TestMessage", -1));
  ASSERT_TRUE(type.has_value());
  EXPECT_EQ(type->second.name(), "google.api.expr.runtime.TestMessage");
}

TEST_F(ResolverTest, TestFindDescriptorNotFound) {
  CelFunctionRegistry func_registry;
  Resolver resolver("google.api.expr.runtime",
                    func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  ASSERT_OK_AND_ASSIGN(auto type, resolver.FindType("UndefinedMessage", -1));
  EXPECT_FALSE(type.has_value()) << type->second;
}

TEST_F(ResolverTest, TestFindOverloads) {
  CelFunctionRegistry func_registry;
  auto status =
      func_registry.Register(std::make_unique<FakeFunction>("fake_func"));
  ASSERT_OK(status);
  status = func_registry.Register(
      std::make_unique<FakeFunction>("cel.fake_ns_func"));
  ASSERT_OK(status);

  Resolver resolver("cel", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto overloads =
      resolver.FindOverloads("fake_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
  EXPECT_THAT(overloads[0].descriptor.name(), Eq("fake_func"));

  overloads =
      resolver.FindOverloads("fake_ns_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
  EXPECT_THAT(overloads[0].descriptor.name(), Eq("cel.fake_ns_func"));
}

TEST_F(ResolverTest, TestFindLazyOverloads) {
  CelFunctionRegistry func_registry;
  auto status = func_registry.RegisterLazyFunction(
      CelFunctionDescriptor{"fake_lazy_func", false, {}});
  ASSERT_OK(status);
  status = func_registry.RegisterLazyFunction(
      CelFunctionDescriptor{"cel.fake_lazy_ns_func", false, {}});
  ASSERT_OK(status);

  Resolver resolver("cel", func_registry.InternalGetRegistry(),
                    type_registry_.InternalGetModernRegistry(),
                    type_registry_.GetTypeProvider());

  auto overloads =
      resolver.FindLazyOverloads("fake_lazy_func", false, ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));

  overloads = resolver.FindLazyOverloads("fake_lazy_ns_func", false,
                                         ArgumentsMatcher(0));
  EXPECT_THAT(overloads.size(), Eq(1));
}

}  // namespace

}  // namespace google::api::expr::runtime
