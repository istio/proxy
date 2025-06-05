// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/source_position.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "internal/testing.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::testing::Eq;
using google::api::expr::v1alpha1::SourceInfo;

class SourcePositionTest : public testing::Test {
 protected:
  void SetUp() override {
    // Simulate the expression positions : '\n\na\n&& b\n\n|| c'
    //
    // Within the ExprChecker, the line offset is the first character of the
    // line rather than the newline character.
    //
    // The tests outputs are affected by leading newlines, but not trailing
    // newlines, and the ExprChecker will actually always generate a trailing
    // newline entry for EOF; however, this offset is not included in the test
    // since there may be other parsers which generate newline information
    // slightly differently.
    source_info_.add_line_offsets(0);
    source_info_.add_line_offsets(1);
    source_info_.add_line_offsets(2);
    (*source_info_.mutable_positions())[1] = 2;
    source_info_.add_line_offsets(4);
    (*source_info_.mutable_positions())[2] = 4;
    (*source_info_.mutable_positions())[3] = 7;
    source_info_.add_line_offsets(9);
    source_info_.add_line_offsets(10);
    (*source_info_.mutable_positions())[4] = 10;
    (*source_info_.mutable_positions())[5] = 13;
  }

  SourceInfo source_info_;
};

TEST_F(SourcePositionTest, TestNullSourceInfo) {
  SourcePosition position(3, nullptr);
  EXPECT_THAT(position.character_offset(), Eq(0));
  EXPECT_THAT(position.line(), Eq(1));
  EXPECT_THAT(position.column(), Eq(1));
}

TEST_F(SourcePositionTest, TestNoNewlines) {
  source_info_.clear_line_offsets();
  SourcePosition position(3, &source_info_);
  EXPECT_THAT(position.character_offset(), Eq(7));
  EXPECT_THAT(position.line(), Eq(1));
  EXPECT_THAT(position.column(), Eq(8));
}

TEST_F(SourcePositionTest, TestPosition) {
  SourcePosition position(3, &source_info_);
  EXPECT_THAT(position.character_offset(), Eq(7));
}

TEST_F(SourcePositionTest, TestLine) {
  SourcePosition position1(1, &source_info_);
  EXPECT_THAT(position1.line(), Eq(3));

  SourcePosition position2(2, &source_info_);
  EXPECT_THAT(position2.line(), Eq(4));

  SourcePosition position3(3, &source_info_);
  EXPECT_THAT(position3.line(), Eq(4));

  SourcePosition position4(5, &source_info_);
  EXPECT_THAT(position4.line(), Eq(6));
}

TEST_F(SourcePositionTest, TestColumn) {
  SourcePosition position1(1, &source_info_);
  EXPECT_THAT(position1.column(), Eq(1));

  SourcePosition position2(2, &source_info_);
  EXPECT_THAT(position2.column(), Eq(1));

  SourcePosition position3(3, &source_info_);
  EXPECT_THAT(position3.column(), Eq(4));

  SourcePosition position4(5, &source_info_);
  EXPECT_THAT(position4.column(), Eq(4));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
