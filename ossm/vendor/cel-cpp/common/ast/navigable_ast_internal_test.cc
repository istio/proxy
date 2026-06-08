// Copyright 2025 Google LLC
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
#include "common/ast/navigable_ast_internal.h"

#include <iterator>
#include <vector>

#include "absl/base/casts.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/ast/navigable_ast_kinds.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

struct TestRangeTraits {
  using UnderlyingType = int;
  static double Adapt(const UnderlyingType& value) {
    return static_cast<double>(value) + 0.5;
  }
};

TEST(NavigableAstRangeTest, BasicIteration) {
  std::vector<int> values{1, 2, 3};
  NavigableAstRange<TestRangeTraits> range(absl::MakeConstSpan(values));
  absl::Span<const int> span(values);
  auto it = range.begin();
  EXPECT_EQ(*it, 1.5);
  EXPECT_EQ(*++it, 2.5);
  EXPECT_EQ(*++it, 3.5);
  EXPECT_EQ(++it, range.end());
  EXPECT_EQ(*--it, 3.5);
  EXPECT_EQ(*--it, 2.5);
  EXPECT_EQ(*--it, 1.5);
  EXPECT_EQ(it, range.begin());
}

TEST(NodeKind, Stringify) {
  // Note: the specific values are not important or guaranteed to be stable,
  // they are only intended to make test outputs clearer.
  EXPECT_EQ(absl::StrCat(NodeKind::kConstant), "Constant");
  EXPECT_EQ(absl::StrCat(NodeKind::kIdent), "Ident");
  EXPECT_EQ(absl::StrCat(NodeKind::kSelect), "Select");
  EXPECT_EQ(absl::StrCat(NodeKind::kCall), "Call");
  EXPECT_EQ(absl::StrCat(NodeKind::kList), "List");
  EXPECT_EQ(absl::StrCat(NodeKind::kMap), "Map");
  EXPECT_EQ(absl::StrCat(NodeKind::kStruct), "Struct");
  EXPECT_EQ(absl::StrCat(NodeKind::kComprehension), "Comprehension");
  EXPECT_EQ(absl::StrCat(NodeKind::kUnspecified), "Unspecified");

  EXPECT_EQ(absl::StrCat(absl::bit_cast<NodeKind>(255)),
            "Unknown NodeKind 255");
}

TEST(ChildKind, Stringify) {
  // Note: the specific values are not important or guaranteed to be stable,
  // they are only intended to make test outputs clearer.
  EXPECT_EQ(absl::StrCat(ChildKind::kSelectOperand), "SelectOperand");
  EXPECT_EQ(absl::StrCat(ChildKind::kCallReceiver), "CallReceiver");
  EXPECT_EQ(absl::StrCat(ChildKind::kCallArg), "CallArg");
  EXPECT_EQ(absl::StrCat(ChildKind::kListElem), "ListElem");
  EXPECT_EQ(absl::StrCat(ChildKind::kMapKey), "MapKey");
  EXPECT_EQ(absl::StrCat(ChildKind::kMapValue), "MapValue");
  EXPECT_EQ(absl::StrCat(ChildKind::kStructValue), "StructValue");
  EXPECT_EQ(absl::StrCat(ChildKind::kComprehensionRange), "ComprehensionRange");
  EXPECT_EQ(absl::StrCat(ChildKind::kComprehensionInit), "ComprehensionInit");
  EXPECT_EQ(absl::StrCat(ChildKind::kComprehensionCondition),
            "ComprehensionCondition");
  EXPECT_EQ(absl::StrCat(ChildKind::kComprehensionLoopStep),
            "ComprehensionLoopStep");
  EXPECT_EQ(absl::StrCat(ChildKind::kComprensionResult), "ComprehensionResult");
  EXPECT_EQ(absl::StrCat(ChildKind::kUnspecified), "Unspecified");

  EXPECT_EQ(absl::StrCat(absl::bit_cast<ChildKind>(255)),
            "Unknown ChildKind 255");
}

}  // namespace
}  // namespace cel::common_internal
