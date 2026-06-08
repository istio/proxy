// Copyright 2025 Google LLC
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

#include "checker/type_checker_subset_factory.h"

#include <memory>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/standard_definitions.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

using ::absl_testing::IsOk;

namespace cel {
namespace {

TEST(TypeCheckerSubsetFactoryTest, IncludeOverloadsByIdPredicate) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  absl::string_view allowlist[] = {
      StandardOverloadIds::kNot,
      StandardOverloadIds::kAnd,
      StandardOverloadIds::kOr,
      StandardOverloadIds::kConditional,
      StandardOverloadIds::kEquals,
      StandardOverloadIds::kNotEquals,
      StandardOverloadIds::kNotStrictlyFalse,
  };
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->GetCheckerBuilder().AddLibrarySubset({
                  "stdlib",
                  IncludeOverloadsByIdPredicate(allowlist),
              }),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  ASSERT_OK_AND_ASSIGN(
      ValidationResult r,
      compiler->Compile(
          "!true || !false && (false) ? true : false && 1 == 2 || 3.0 != 2.1"));

  EXPECT_TRUE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(
      r, compiler->Compile("[true, false, true, false].exists(x, x && !x)"));

  EXPECT_TRUE(r.IsValid());

  // Not in allowlist.
  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("1 + 2 < 3"));
  EXPECT_FALSE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("'abc' + 'def'"));
  EXPECT_FALSE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("r'foo.*'.matches('foobar')"));
  EXPECT_FALSE(r.IsValid());
}

TEST(TypeCheckerSubsetFactoryTest, ExcludeOverloadsByIdPredicate) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  absl::string_view exclude_list[] = {
      StandardOverloadIds::kMatches,
      StandardOverloadIds::kMatchesMember,
  };
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());
  ASSERT_THAT(builder->GetCheckerBuilder().AddLibrarySubset({
                  "stdlib",
                  ExcludeOverloadsByIdPredicate(exclude_list),
              }),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  ASSERT_OK_AND_ASSIGN(
      ValidationResult r,
      compiler->Compile(
          "!true || !false && (false) ? true : false && 1 == 2 || 3.0 != 2.1"));

  EXPECT_TRUE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(
      r, compiler->Compile("[true, false, true, false].exists(x, x && !x)"));

  EXPECT_TRUE(r.IsValid());

  // Not in allowlist.
  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("1 + 2 < 3"));
  EXPECT_TRUE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("'abc' + 'def'"));
  EXPECT_TRUE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("r'foo.*'.matches('foobar')"));
  EXPECT_FALSE(r.IsValid());

  ASSERT_OK_AND_ASSIGN(r, compiler->Compile("matches(r'foo.*', 'foobar')"));
  EXPECT_FALSE(r.IsValid());
}

}  // namespace

}  // namespace cel
