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

#include "internal/names.h"

#include "internal/testing.h"

namespace cel::internal {
namespace {

struct NamesTestCase final {
  absl::string_view text;
  bool ok;
};

using IsValidRelativeNameTest = testing::TestWithParam<NamesTestCase>;

TEST_P(IsValidRelativeNameTest, Compliance) {
  const NamesTestCase& test_case = GetParam();
  if (test_case.ok) {
    EXPECT_TRUE(IsValidRelativeName(test_case.text));
  } else {
    EXPECT_FALSE(IsValidRelativeName(test_case.text));
  }
}

INSTANTIATE_TEST_SUITE_P(IsValidRelativeNameTest, IsValidRelativeNameTest,
                         testing::ValuesIn<NamesTestCase>({{"foo", true},
                                                           {"foo.Bar", true},
                                                           {"", false},
                                                           {".", false},
                                                           {".foo", false},
                                                           {".foo.Bar", false},
                                                           {"foo..Bar", false},
                                                           {"foo.Bar.",
                                                            false}}));

}  // namespace
}  // namespace cel::internal
