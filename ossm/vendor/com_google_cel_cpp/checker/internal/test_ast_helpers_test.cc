// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/test_ast_helpers.h"

#include <memory>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "base/ast_internal/ast_impl.h"
#include "common/ast.h"
#include "internal/testing.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::StatusIs;
using ::cel::ast_internal::AstImpl;

TEST(MakeTestParsedAstTest, Works) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Ast> ast, MakeTestParsedAst("123"));
  AstImpl& impl = AstImpl::CastFromPublicAst(*ast);
  EXPECT_TRUE(impl.root_expr().has_const_expr());
}

TEST(MakeTestParsedAstTest, ForwardsParseError) {
  EXPECT_THAT(MakeTestParsedAst("%123"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel::checker_internal
