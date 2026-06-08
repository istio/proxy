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

#include "compiler/compiler_library_subset_factory.h"

#include <memory>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "checker/validation_result.h"
#include "common/standard_definitions.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"

using ::absl_testing::IsOk;
using ::testing::Not;

namespace cel {
namespace {

MATCHER(IsValid, "") {
  const absl::StatusOr<ValidationResult>& result = arg;
  if (!result.ok()) {
    (*result_listener) << "compilation failed: " << result.status();
    return false;
  }
  if (!result->GetIssues().empty()) {
    (*result_listener) << "compilation issues: \n" << result->FormatError();
  }
  return result->IsValid();
}

TEST(CompilerLibrarySubsetFactoryTest, MakeStdlibSubsetInclude) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  ASSERT_THAT(
      builder->AddLibrarySubset(MakeStdlibSubset(
          {"exists", "all"},
          {StandardOverloadIds::kAnd, StandardOverloadIds::kOr,
           StandardOverloadIds::kNot, StandardOverloadIds::kNotStrictlyFalse,
           StandardOverloadIds::kEquals, StandardOverloadIds::kNotEquals})),
      IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  EXPECT_THAT(
      compiler->Compile(
          "[1, 2, 3].exists(x, x != 1 || x == 2 && !(x == 4 || x == 5) )"),
      IsValid());
  EXPECT_THAT(compiler->Compile("1+2"), Not(IsValid()));
  EXPECT_THAT(compiler->Compile("[1, 2, 3].map(x, x)"), Not(IsValid()));
}

TEST(CompilerLibrarySubsetFactoryTest, MakeStdlibSubsetExclude) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  ASSERT_THAT(builder->AddLibrarySubset(MakeStdlibSubset(
                  absl::flat_hash_set<std::string>({"map"}), {"add_list"},
                  {.macro_list = StdlibSubsetOptions::ListKind::kExclude,
                   .function_list = StdlibSubsetOptions::ListKind::kExclude})),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  EXPECT_THAT(
      compiler->Compile(
          "[1, 2, 3].exists(x, x != 1 || x == 2 && !(x == 4 || x == 5) )"),
      IsValid());
  EXPECT_THAT(compiler->Compile("1+2"), IsValid());
  EXPECT_THAT(compiler->Compile("[1, 2, 3].map(x, x)"), Not(IsValid()));
  EXPECT_THAT(compiler->Compile("[2] + [1]"), Not(IsValid()));
}

TEST(CompilerLibrarySubsetFactoryTest, MakeStdlibSubsetByMacroName) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  absl::string_view kMacroNames[] = {"map"};
  ASSERT_THAT(builder->AddLibrarySubset(MakeStdlibSubsetByMacroName(
                  kMacroNames,
                  {.macro_list = StdlibSubsetOptions::ListKind::kExclude})),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  EXPECT_THAT(
      compiler->Compile(
          "[1, 2, 3].exists(x, x != 1 || x == 2 && !(x == 4 || x == 5) )"),
      IsValid());
  EXPECT_THAT(compiler->Compile("1+2"), IsValid());
  EXPECT_THAT(compiler->Compile("[1, 2, 3].map(x, x)"), Not(IsValid()));
  EXPECT_THAT(compiler->Compile("[2] + [1]"), IsValid());
}

TEST(CompilerLibrarySubsetFactoryTest, MakeStdlibSubsetByOverloadId) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CompilerBuilder> builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(StandardCompilerLibrary()), IsOk());

  absl::string_view kOverloadIds[] = {"add_list", "add_string"};
  ASSERT_THAT(builder->AddLibrarySubset(MakeStdlibSubsetByOverloadId(
                  kOverloadIds,
                  {// unused
                   .macro_list = StdlibSubsetOptions::ListKind::kInclude,
                   .function_list = StdlibSubsetOptions::ListKind::kExclude})),
              IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Compiler> compiler, builder->Build());

  EXPECT_THAT(
      compiler->Compile(
          "[1, 2, 3].exists(x, x != 1 || x == 2 && !(x == 4 || x == 5) )"),
      IsValid());
  EXPECT_THAT(compiler->Compile("1+2"), IsValid());
  EXPECT_THAT(compiler->Compile("[1, 2, 3].map(x, x)"), Not(IsValid()));
  EXPECT_THAT(compiler->Compile("[2] + [1]"), Not(IsValid()));
}

}  // namespace
}  // namespace cel
