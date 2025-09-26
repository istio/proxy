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

#include "internal/lexis.h"

#include "internal/testing.h"

namespace cel::internal {
namespace {

struct LexisTestCase final {
  absl::string_view text;
  bool ok;
};

using LexisIsReservedTest = testing::TestWithParam<LexisTestCase>;

TEST_P(LexisIsReservedTest, Compliance) {
  const LexisTestCase& test_case = GetParam();
  if (test_case.ok) {
    EXPECT_TRUE(LexisIsReserved(test_case.text));
  } else {
    EXPECT_FALSE(LexisIsReserved(test_case.text));
  }
}

INSTANTIATE_TEST_SUITE_P(LexisIsReservedTest, LexisIsReservedTest,
                         testing::ValuesIn<LexisTestCase>({{"true", true},
                                                           {"cel", false}}));

using LexisIsIdentifierTest = testing::TestWithParam<LexisTestCase>;

TEST_P(LexisIsIdentifierTest, Compliance) {
  const LexisTestCase& test_case = GetParam();
  if (test_case.ok) {
    EXPECT_TRUE(LexisIsIdentifier(test_case.text));
  } else {
    EXPECT_FALSE(LexisIsIdentifier(test_case.text));
  }
}

INSTANTIATE_TEST_SUITE_P(
    LexisIsIdentifierTest, LexisIsIdentifierTest,
    testing::ValuesIn<LexisTestCase>(
        {{"true", false},    {"0abc", false},    {"-abc", false},
         {".abc", false},    {"~abc", false},    {"!abc", false},
         {"abc-", false},    {"abc.", false},    {"abc~", false},
         {"abc!", false},    {"cel", true},      {"cel0", true},
         {"_cel", true},     {"_cel0", true},    {"cel_", true},
         {"cel0_", true},    {"cel_cel", true},  {"cel0_cel", true},
         {"cel_cel0", true}, {"cel0_cel0", true}}));

}  // namespace
}  // namespace cel::internal
