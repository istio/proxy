// Copyright 2023 Google LLC
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

#include "runtime/activation.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Optional;
using testing::SizeIs;
using testing::Truly;
using testing::UnorderedElementsAre;

MATCHER_P(IsIntValue, x, absl::StrCat("is IntValue Handle with value ", x)) {
  const Value& handle = arg;

  return handle->Is<IntValue>() && handle.GetInt().NativeValue() == x;
}

MATCHER_P(AttributePatternMatches, val, "matches AttributePattern") {
  const AttributePattern& pattern = arg;
  const Attribute& expected = val;

  return pattern.IsMatch(expected) == AttributePattern::MatchType::FULL;
}

class FunctionImpl : public cel::Function {
 public:
  FunctionImpl() = default;

  absl::StatusOr<Value> Invoke(absl::Span<const Value> args,
                               const google::protobuf::DescriptorPool* absl_nonnull,
                               google::protobuf::MessageFactory* absl_nonnull,
                               google::protobuf::Arena* absl_nonnull) const override {
    return NullValue();
  }
};

using ActivationTest = common_internal::ValueTest<>;

TEST_F(ActivationTest, ValueNotFound) {
  Activation activation;

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ActivationTest, InsertValue) {
  Activation activation;
  EXPECT_TRUE(activation.InsertOrAssignValue("var1", IntValue(42)));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
}

TEST_F(ActivationTest, InsertValueOverwrite) {
  Activation activation;
  EXPECT_TRUE(activation.InsertOrAssignValue("var1", IntValue(42)));
  EXPECT_FALSE(activation.InsertOrAssignValue("var1", IntValue(0)));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(0))));
}

TEST_F(ActivationTest, InsertProvider) {
  Activation activation;

  EXPECT_TRUE(activation.InsertOrAssignValueProvider(
      "var1",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull,
         google::protobuf::Arena* absl_nonnull) { return IntValue(42); }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
}

TEST_F(ActivationTest, InsertProviderForwardsNotFound) {
  Activation activation;

  EXPECT_TRUE(activation.InsertOrAssignValueProvider(
      "var1",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull,
         google::protobuf::Arena* absl_nonnull) { return absl::nullopt; }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ActivationTest, InsertProviderForwardsStatus) {
  Activation activation;

  EXPECT_TRUE(activation.InsertOrAssignValueProvider(
      "var1",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull,
         google::protobuf::Arena* absl_nonnull) { return absl::InternalError("test"); }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              StatusIs(absl::StatusCode::kInternal, "test"));
}

TEST_F(ActivationTest, ProviderMemoized) {
  Activation activation;
  int call_count = 0;

  EXPECT_TRUE(activation.InsertOrAssignValueProvider(
      "var1", [&call_count](absl::string_view name,
                            const google::protobuf::DescriptorPool* absl_nonnull,
                            google::protobuf::MessageFactory* absl_nonnull,
                            google::protobuf::Arena* absl_nonnull) {
        call_count++;
        return IntValue(42);
      }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_EQ(call_count, 1);
}

TEST_F(ActivationTest, InsertProviderOverwrite) {
  Activation activation;

  EXPECT_TRUE(activation.InsertOrAssignValueProvider(
      "var1",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull,
         google::protobuf::Arena* absl_nonnull) { return IntValue(42); }));
  EXPECT_FALSE(activation.InsertOrAssignValueProvider(
      "var1",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull,
         google::protobuf::Arena* absl_nonnull) { return IntValue(0); }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(0))));
}

TEST_F(ActivationTest, ValuesAndProvidersShareNamespace) {
  Activation activation;
  bool called = false;

  EXPECT_TRUE(activation.InsertOrAssignValue("var1", IntValue(41)));
  EXPECT_TRUE(activation.InsertOrAssignValue("var2", IntValue(41)));

  EXPECT_FALSE(activation.InsertOrAssignValueProvider(
      "var1", [&called](absl::string_view name,
                        const google::protobuf::DescriptorPool* absl_nonnull,
                        google::protobuf::MessageFactory* absl_nonnull,
                        google::protobuf::Arena* absl_nonnull) {
        called = true;
        return IntValue(42);
      }));

  EXPECT_THAT(activation.FindVariable("var1", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(activation.FindVariable("var2", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(41))));
  EXPECT_TRUE(called);
}

TEST_F(ActivationTest, SetUnknownAttributes) {
  Activation activation;

  activation.SetUnknownPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});

  EXPECT_THAT(
      activation.GetUnknownAttributes(),
      ElementsAre(AttributePatternMatches(Attribute(
                      "var1", {AttributeQualifier::OfString("field1")})),
                  AttributePatternMatches(Attribute(
                      "var1", {AttributeQualifier::OfString("field2")}))));
}

TEST_F(ActivationTest, ClearUnknownAttributes) {
  Activation activation;

  activation.SetUnknownPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});
  activation.SetUnknownPatterns({});

  EXPECT_THAT(activation.GetUnknownAttributes(), IsEmpty());
}

TEST_F(ActivationTest, SetMissingAttributes) {
  Activation activation;

  activation.SetMissingPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});

  EXPECT_THAT(
      activation.GetMissingAttributes(),
      ElementsAre(AttributePatternMatches(Attribute(
                      "var1", {AttributeQualifier::OfString("field1")})),
                  AttributePatternMatches(Attribute(
                      "var1", {AttributeQualifier::OfString("field2")}))));
}

TEST_F(ActivationTest, ClearMissingAttributes) {
  Activation activation;

  activation.SetMissingPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});
  activation.SetMissingPatterns({});

  EXPECT_THAT(activation.GetMissingAttributes(), IsEmpty());
}

TEST_F(ActivationTest, InsertFunctionOk) {
  Activation activation;

  EXPECT_TRUE(
      activation.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kUint}),
                                std::make_unique<FunctionImpl>()));
  EXPECT_TRUE(
      activation.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kInt}),
                                std::make_unique<FunctionImpl>()));
  EXPECT_TRUE(
      activation.InsertFunction(FunctionDescriptor("Fn2", false, {Kind::kInt}),
                                std::make_unique<FunctionImpl>()));

  EXPECT_THAT(
      activation.FindFunctionOverloads("Fn"),
      UnorderedElementsAre(
          Truly([](const FunctionOverloadReference& ref) {
            return ref.descriptor.name() == "Fn" &&
                   ref.descriptor.types() == std::vector<Kind>{Kind::kUint};
          }),
          Truly([](const FunctionOverloadReference& ref) {
            return ref.descriptor.name() == "Fn" &&
                   ref.descriptor.types() == std::vector<Kind>{Kind::kInt};
          })))
      << "expected overloads Fn(int), Fn(uint)";
}

TEST_F(ActivationTest, InsertFunctionFails) {
  Activation activation;

  EXPECT_TRUE(
      activation.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kAny}),
                                std::make_unique<FunctionImpl>()));
  EXPECT_FALSE(
      activation.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kInt}),
                                std::make_unique<FunctionImpl>()));

  EXPECT_THAT(activation.FindFunctionOverloads("Fn"),
              ElementsAre(Truly([](const FunctionOverloadReference& ref) {
                return ref.descriptor.name() == "Fn" &&
                       ref.descriptor.types() == std::vector<Kind>{Kind::kAny};
              })))
      << "expected overload Fn(any)";
}

TEST_F(ActivationTest, MoveAssignment) {
  Activation moved_from;

  ASSERT_TRUE(
      moved_from.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kAny}),
                                std::make_unique<FunctionImpl>()));
  ASSERT_TRUE(moved_from.InsertOrAssignValue("val", IntValue(42)));

  ASSERT_TRUE(moved_from.InsertOrAssignValueProvider(
      "val_provided",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull)
          -> absl::StatusOr<absl::optional<Value>> { return IntValue(42); }));
  moved_from.SetUnknownPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});
  moved_from.SetMissingPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});

  Activation moved_to;
  moved_to = std::move(moved_from);

  EXPECT_THAT(moved_to.FindVariable("val", descriptor_pool(), message_factory(),
                                    arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(moved_to.FindVariable("val_provided", descriptor_pool(),
                                    message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(moved_to.FindFunctionOverloads("Fn"), SizeIs(1));
  EXPECT_THAT(moved_to.GetUnknownAttributes(), SizeIs(2));
  EXPECT_THAT(moved_to.GetMissingAttributes(), SizeIs(2));

  // moved from value is empty. (well defined but not specified state)
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_THAT(moved_from.FindVariable("val", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(moved_from.FindVariable("val_provided", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(moved_from.FindFunctionOverloads("Fn"), SizeIs(0));
  EXPECT_THAT(moved_from.GetUnknownAttributes(), SizeIs(0));
  EXPECT_THAT(moved_from.GetMissingAttributes(), SizeIs(0));
  // NOLINTEND(bugprone-use-after-move)
}

TEST_F(ActivationTest, MoveCtor) {
  Activation moved_from;

  ASSERT_TRUE(
      moved_from.InsertFunction(FunctionDescriptor("Fn", false, {Kind::kAny}),
                                std::make_unique<FunctionImpl>()));
  ASSERT_TRUE(moved_from.InsertOrAssignValue("val", IntValue(42)));

  ASSERT_TRUE(moved_from.InsertOrAssignValueProvider(
      "val_provided",
      [](absl::string_view name, const google::protobuf::DescriptorPool* absl_nonnull,
         google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull)
          -> absl::StatusOr<absl::optional<Value>> { return IntValue(42); }));
  moved_from.SetUnknownPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});
  moved_from.SetMissingPatterns(
      {AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field1")}),
       AttributePattern("var1",
                        {AttributeQualifierPattern::OfString("field2")})});

  Activation moved_to = std::move(moved_from);

  EXPECT_THAT(moved_to.FindVariable("val", descriptor_pool(), message_factory(),
                                    arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(moved_to.FindVariable("val_provided", descriptor_pool(),
                                    message_factory(), arena()),
              IsOkAndHolds(Optional(IsIntValue(42))));
  EXPECT_THAT(moved_to.FindFunctionOverloads("Fn"), SizeIs(1));
  EXPECT_THAT(moved_to.GetUnknownAttributes(), SizeIs(2));
  EXPECT_THAT(moved_to.GetMissingAttributes(), SizeIs(2));

  // moved from value is empty.
  // NOLINTBEGIN(bugprone-use-after-move)
  EXPECT_THAT(moved_from.FindVariable("val", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(moved_from.FindVariable("val_provided", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(moved_from.FindFunctionOverloads("Fn"), SizeIs(0));
  EXPECT_THAT(moved_from.GetUnknownAttributes(), SizeIs(0));
  EXPECT_THAT(moved_from.GetMissingAttributes(), SizeIs(0));
  // NOLINTEND(bugprone-use-after-move)
}

}  // namespace
}  // namespace cel
