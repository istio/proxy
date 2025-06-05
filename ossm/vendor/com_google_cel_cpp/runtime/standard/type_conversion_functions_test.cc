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

#include "runtime/standard/type_conversion_functions.h"

#include <vector>

#include "base/builtins.h"
#include "base/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

MATCHER_P3(MatchesUnaryDescriptor, name, receiver, expected_kind, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;
  std::vector<Kind> types{expected_kind};
  return descriptor.name() == name && descriptor.receiver_style() == receiver &&
         descriptor.types() == types;
}

TEST(RegisterTypeConversionFunctions, RegisterBoolConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kBool, false, {Kind::kAny}),
              UnorderedElementsAre(
                  MatchesUnaryDescriptor(builtin::kBool, false, Kind::kBool)));
}

TEST(RegisterTypeConversionFunctions, RegisterIntConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kInt, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kInt),
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kDouble),
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kUint),
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kBool),
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kString),
          MatchesUnaryDescriptor(builtin::kInt, false, Kind::kTimestamp)));
}

TEST(RegisterTypeConversionFunctions, RegisterUintConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kUint, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kUint, false, Kind::kInt),
          MatchesUnaryDescriptor(builtin::kUint, false, Kind::kDouble),
          MatchesUnaryDescriptor(builtin::kUint, false, Kind::kUint),
          MatchesUnaryDescriptor(builtin::kUint, false, Kind::kString)));
}

TEST(RegisterTypeConversionFunctions, RegisterDoubleConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kDouble, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kDouble, false, Kind::kInt),
          MatchesUnaryDescriptor(builtin::kDouble, false, Kind::kDouble),
          MatchesUnaryDescriptor(builtin::kDouble, false, Kind::kUint),
          MatchesUnaryDescriptor(builtin::kDouble, false, Kind::kString)));
}

TEST(RegisterTypeConversionFunctions, RegisterStringConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  options.enable_string_conversion = true;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kString, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kInt),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kDouble),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kUint),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kString),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kBytes),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kDuration),
          MatchesUnaryDescriptor(builtin::kString, false, Kind::kTimestamp)));
}

TEST(RegisterTypeConversionFunctions,
     RegisterStringConversionFunctionsDisabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_string_conversion = false;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kString, false, {Kind::kAny}),
      IsEmpty());
}

TEST(RegisterTypeConversionFunctions, RegisterBytesConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kBytes, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kBytes, false, Kind::kBytes),
          MatchesUnaryDescriptor(builtin::kBytes, false, Kind::kString)));
}

TEST(RegisterTypeConversionFunctions, RegisterTimeConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kTimestamp, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kTimestamp, false, Kind::kInt),
          MatchesUnaryDescriptor(builtin::kTimestamp, false, Kind::kString),
          MatchesUnaryDescriptor(builtin::kTimestamp, false,
                                 Kind::kTimestamp)));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kDuration, false, {Kind::kAny}),
      UnorderedElementsAre(
          MatchesUnaryDescriptor(builtin::kDuration, false, Kind::kString),
          MatchesUnaryDescriptor(builtin::kDuration, false, Kind::kDuration)));
}

TEST(RegisterTypeConversionFunctions, RegisterMetaTypeConversionFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTypeConversionFunctions(registry, options));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kDyn, false, {Kind::kAny}),
              UnorderedElementsAre(
                  MatchesUnaryDescriptor(builtin::kDyn, false, Kind::kAny)));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kType, false, {Kind::kAny}),
              UnorderedElementsAre(
                  MatchesUnaryDescriptor(builtin::kType, false, Kind::kAny)));
}

// TODO: move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
