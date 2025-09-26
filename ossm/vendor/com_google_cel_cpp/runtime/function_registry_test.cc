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

#include "runtime/function_registry.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/function.h"
#include "runtime/function_adapter.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_provider.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::absl_testing::StatusIs;
using ::cel::runtime_internal::FunctionProvider;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::Truly;

class ConstIntFunction : public cel::Function {
 public:
  static cel::FunctionDescriptor MakeDescriptor() {
    return {"ConstFunction", false, {}};
  }

  absl::StatusOr<Value> Invoke(
      absl::Span<const Value> args,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    return IntValue(42);
  }
};

TEST(FunctionRegistryTest, InsertAndRetrieveLazyFunction) {
  cel::FunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  FunctionRegistry registry;
  Activation activation;
  ASSERT_OK(registry.RegisterLazyFunction(lazy_function_desc));

  const auto descriptors =
      registry.FindLazyOverloads("LazyFunction", false, {});
  EXPECT_THAT(descriptors, SizeIs(1));
}

// Confirm that lazy and static functions share the same descriptor space:
// i.e. you can't insert both a lazy function and a static function for the same
// descriptors.
TEST(FunctionRegistryTest, LazyAndStaticFunctionShareDescriptorSpace) {
  FunctionRegistry registry;
  cel::FunctionDescriptor desc = ConstIntFunction::MakeDescriptor();
  ASSERT_OK(registry.RegisterLazyFunction(desc));

  absl::Status status = registry.Register(ConstIntFunction::MakeDescriptor(),
                                          std::make_unique<ConstIntFunction>());
  EXPECT_FALSE(status.ok());
}

TEST(FunctionRegistryTest, FindStaticOverloadsReturns) {
  FunctionRegistry registry;
  cel::FunctionDescriptor desc = ConstIntFunction::MakeDescriptor();
  ASSERT_OK(registry.Register(desc, std::make_unique<ConstIntFunction>()));

  std::vector<cel::FunctionOverloadReference> overloads =
      registry.FindStaticOverloads(desc.name(), false, {});

  EXPECT_THAT(overloads,
              ElementsAre(Truly(
                  [](const cel::FunctionOverloadReference& overload) -> bool {
                    return overload.descriptor.name() == "ConstFunction";
                  })))
      << "Expected single ConstFunction()";
}

TEST(FunctionRegistryTest, ListFunctions) {
  cel::FunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  FunctionRegistry registry;

  ASSERT_OK(registry.RegisterLazyFunction(lazy_function_desc));
  EXPECT_OK(registry.Register(ConstIntFunction::MakeDescriptor(),
                              std::make_unique<ConstIntFunction>()));

  auto registered_functions = registry.ListFunctions();

  EXPECT_THAT(registered_functions, SizeIs(2));
  EXPECT_THAT(registered_functions["LazyFunction"], SizeIs(1));
  EXPECT_THAT(registered_functions["ConstFunction"], SizeIs(1));
}

TEST(FunctionRegistryTest, DefaultLazyProviderNoOverloadFound) {
  FunctionRegistry registry;
  Activation activation;
  cel::FunctionDescriptor lazy_function_desc{"LazyFunction", false, {}};
  EXPECT_OK(registry.RegisterLazyFunction(lazy_function_desc));

  auto providers = registry.FindLazyOverloads("LazyFunction", false, {});
  ASSERT_THAT(providers, SizeIs(1));
  const FunctionProvider& provider = providers[0].provider;
  ASSERT_OK_AND_ASSIGN(
      absl::optional<FunctionOverloadReference> func,
      provider.GetFunction({"LazyFunc", false, {cel::Kind::kInt64}},
                           activation));

  EXPECT_EQ(func, absl::nullopt);
}

TEST(FunctionRegistryTest, DefaultLazyProviderReturnsImpl) {
  FunctionRegistry registry;
  Activation activation;
  EXPECT_OK(registry.RegisterLazyFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kAny})));
  EXPECT_TRUE(activation.InsertFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kInt}),
      UnaryFunctionAdapter<int64_t, int64_t>::WrapFunction(
          [](int64_t x) { return 2 * x; })));
  EXPECT_TRUE(activation.InsertFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kDouble}),
      UnaryFunctionAdapter<double, double>::WrapFunction(
          [](double x) { return 2 * x; })));

  auto providers =
      registry.FindLazyOverloads("LazyFunction", false, {Kind::kInt});
  ASSERT_THAT(providers, SizeIs(1));
  const FunctionProvider& provider = providers[0].provider;
  ASSERT_OK_AND_ASSIGN(
      absl::optional<FunctionOverloadReference> func,
      provider.GetFunction(
          FunctionDescriptor("LazyFunction", false, {Kind::kInt}), activation));

  ASSERT_TRUE(func.has_value());
  EXPECT_EQ(func->descriptor.name(), "LazyFunction");
  EXPECT_EQ(func->descriptor.types(), std::vector{cel::Kind::kInt64});
}

TEST(FunctionRegistryTest, DefaultLazyProviderAmbiguousOverload) {
  FunctionRegistry registry;
  Activation activation;
  EXPECT_OK(registry.RegisterLazyFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kAny})));
  EXPECT_TRUE(activation.InsertFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kInt}),
      UnaryFunctionAdapter<int64_t, int64_t>::WrapFunction(
          [](int64_t x) { return 2 * x; })));
  EXPECT_TRUE(activation.InsertFunction(
      FunctionDescriptor("LazyFunction", false, {Kind::kDouble}),
      UnaryFunctionAdapter<double, double>::WrapFunction(
          [](double x) { return 2 * x; })));

  auto providers =
      registry.FindLazyOverloads("LazyFunction", false, {Kind::kInt});
  ASSERT_THAT(providers, SizeIs(1));
  const FunctionProvider& provider = providers[0].provider;

  EXPECT_THAT(
      provider.GetFunction(
          FunctionDescriptor("LazyFunction", false, {Kind::kAny}), activation),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Couldn't resolve function")));
}

TEST(FunctionRegistryTest, CanRegisterNonStrictFunction) {
  {
    FunctionRegistry registry;
    cel::FunctionDescriptor descriptor("NonStrictFunction",
                                       /*receiver_style=*/false, {Kind::kAny},
                                       /*is_strict=*/false);
    ASSERT_OK(
        registry.Register(descriptor, std::make_unique<ConstIntFunction>()));
    EXPECT_THAT(
        registry.FindStaticOverloads("NonStrictFunction", false, {Kind::kAny}),
        SizeIs(1));
  }
  {
    FunctionRegistry registry;
    cel::FunctionDescriptor descriptor("NonStrictLazyFunction",
                                       /*receiver_style=*/false, {Kind::kAny},
                                       /*is_strict=*/false);
    EXPECT_OK(registry.RegisterLazyFunction(descriptor));
    EXPECT_THAT(registry.FindLazyOverloads("NonStrictLazyFunction", false,
                                           {Kind::kAny}),
                SizeIs(1));
  }
}

using NonStrictTestCase = std::tuple<bool, bool>;
using NonStrictRegistrationFailTest = testing::TestWithParam<NonStrictTestCase>;

TEST_P(NonStrictRegistrationFailTest,
       IfOtherOverloadExistsRegisteringNonStrictFails) {
  bool existing_function_is_lazy, new_function_is_lazy;
  std::tie(existing_function_is_lazy, new_function_is_lazy) = GetParam();
  FunctionRegistry registry;
  cel::FunctionDescriptor descriptor("OverloadedFunction",
                                     /*receiver_style=*/false, {Kind::kAny},
                                     /*is_strict=*/true);
  if (existing_function_is_lazy) {
    ASSERT_OK(registry.RegisterLazyFunction(descriptor));
  } else {
    ASSERT_OK(
        registry.Register(descriptor, std::make_unique<ConstIntFunction>()));
  }
  cel::FunctionDescriptor new_descriptor("OverloadedFunction",
                                         /*receiver_style=*/false,
                                         {Kind::kAny, Kind::kAny},
                                         /*is_strict=*/false);
  absl::Status status;
  if (new_function_is_lazy) {
    status = registry.RegisterLazyFunction(new_descriptor);
  } else {
    status =
        registry.Register(new_descriptor, std::make_unique<ConstIntFunction>());
  }
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kAlreadyExists,
                               HasSubstr("Only one overload")));
}

TEST_P(NonStrictRegistrationFailTest,
       IfOtherNonStrictExistsRegisteringStrictFails) {
  bool existing_function_is_lazy, new_function_is_lazy;
  std::tie(existing_function_is_lazy, new_function_is_lazy) = GetParam();
  FunctionRegistry registry;
  cel::FunctionDescriptor descriptor("OverloadedFunction",
                                     /*receiver_style=*/false, {Kind::kAny},
                                     /*is_strict=*/false);
  if (existing_function_is_lazy) {
    ASSERT_OK(registry.RegisterLazyFunction(descriptor));
  } else {
    ASSERT_OK(
        registry.Register(descriptor, std::make_unique<ConstIntFunction>()));
  }
  cel::FunctionDescriptor new_descriptor("OverloadedFunction",
                                         /*receiver_style=*/false,
                                         {Kind::kAny, Kind::kAny},
                                         /*is_strict=*/true);
  absl::Status status;
  if (new_function_is_lazy) {
    status = registry.RegisterLazyFunction(new_descriptor);
  } else {
    status =
        registry.Register(new_descriptor, std::make_unique<ConstIntFunction>());
  }
  EXPECT_THAT(status, StatusIs(absl::StatusCode::kAlreadyExists,
                               HasSubstr("Only one overload")));
}

TEST_P(NonStrictRegistrationFailTest, CanRegisterStrictFunctionsWithoutLimit) {
  bool existing_function_is_lazy, new_function_is_lazy;
  std::tie(existing_function_is_lazy, new_function_is_lazy) = GetParam();
  FunctionRegistry registry;
  cel::FunctionDescriptor descriptor("OverloadedFunction",
                                     /*receiver_style=*/false, {Kind::kAny},
                                     /*is_strict=*/true);
  if (existing_function_is_lazy) {
    ASSERT_OK(registry.RegisterLazyFunction(descriptor));
  } else {
    ASSERT_OK(
        registry.Register(descriptor, std::make_unique<ConstIntFunction>()));
  }
  cel::FunctionDescriptor new_descriptor("OverloadedFunction",
                                         /*receiver_style=*/false,
                                         {Kind::kAny, Kind::kAny},
                                         /*is_strict=*/true);
  absl::Status status;
  if (new_function_is_lazy) {
    status = registry.RegisterLazyFunction(new_descriptor);
  } else {
    status =
        registry.Register(new_descriptor, std::make_unique<ConstIntFunction>());
  }
  EXPECT_OK(status);
}

INSTANTIATE_TEST_SUITE_P(NonStrictRegistrationFailTest,
                         NonStrictRegistrationFailTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace

}  // namespace cel
