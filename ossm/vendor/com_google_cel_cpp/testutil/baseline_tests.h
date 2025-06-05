// Copyright 2024 Google LLC
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
//
// Utilities for baseline tests. Baseline files are textual reports in a common
// format that can be used to compare the output of each of the libraries.
//
// The protobuf ast format is a bit tricky to compare directly (e.g.
// renumberings do not change the meaning of the expression), so we use a custom
// format that compares well with simple string comparisons.
//
// Example:
// ```
// Source: Foo(a.b)
// declare a {
//   variable map(string,dyn)
// }
// declare Foo {
//   function foo_string(string) -> string
//   function foo_int(int) -> int
// }
// =========>
// Foo(
//   a~map(string,dyn)^a.b~dyn
// )~dyn^foo_string|foo_int
//
//
// ```
#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_BASELINE_TESTS_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_BASELINE_TESTS_H_

#include <string>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "common/ast.h"

namespace cel::test {

std::string FormatBaselineAst(const Ast& ast);

std::string FormatBaselineCheckedExpr(
    const google::api::expr::v1alpha1::CheckedExpr& checked);

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_BASELINE_TEST_H_
