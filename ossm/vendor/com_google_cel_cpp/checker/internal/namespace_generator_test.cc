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

#include "checker/internal/namespace_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel::checker_internal {
namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::Pair;

TEST(NamespaceGeneratorTest, EmptyContainer) {
  ASSERT_OK_AND_ASSIGN(auto generator, NamespaceGenerator::Create(""));
  std::vector<std::string> candidates;
  generator.GenerateCandidates("foo", [&](absl::string_view candidate) {
    candidates.push_back(std::string(candidate));
    return true;
  });
  EXPECT_THAT(candidates, ElementsAre("foo"));
}

TEST(NamespaceGeneratorTest, MultipleSegments) {
  ASSERT_OK_AND_ASSIGN(auto generator,
                       NamespaceGenerator::Create("com.example"));
  std::vector<std::string> candidates;
  generator.GenerateCandidates("foo", [&](absl::string_view candidate) {
    candidates.push_back(std::string(candidate));
    return true;
  });
  EXPECT_THAT(candidates, ElementsAre("com.example.foo", "com.foo", "foo"));
}

TEST(NamespaceGeneratorTest, MultipleSegmentsRootNamespace) {
  ASSERT_OK_AND_ASSIGN(auto generator,
                       NamespaceGenerator::Create("com.example"));
  std::vector<std::string> candidates;
  generator.GenerateCandidates(".foo", [&](absl::string_view candidate) {
    candidates.push_back(std::string(candidate));
    return true;
  });
  EXPECT_THAT(candidates, ElementsAre("foo"));
}

TEST(NamespaceGeneratorTest, InvalidContainers) {
  EXPECT_THAT(NamespaceGenerator::Create(".com.example"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(NamespaceGenerator::Create("com..example"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(NamespaceGenerator::Create("com.$example"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(NamespaceGeneratorTest, MultipleSegmentsSelectInterpretation) {
  ASSERT_OK_AND_ASSIGN(auto generator,
                       NamespaceGenerator::Create("com.example"));
  std::vector<std::string> qualified_ident = {"foo", "Bar"};
  std::vector<std::pair<std::string, int>> candidates;
  generator.GenerateCandidates(
      qualified_ident, [&](absl::string_view candidate, int segment_index) {
        candidates.push_back(std::pair(std::string(candidate), segment_index));
        return true;
      });
  EXPECT_THAT(
      candidates,
      ElementsAre(Pair("com.example.foo.Bar", 1), Pair("com.example.foo", 0),
                  Pair("com.foo.Bar", 1), Pair("com.foo", 0),
                  Pair("foo.Bar", 1), Pair("foo", 0)));
}

TEST(NamespaceGeneratorTest,
     MultipleSegmentsSelectInterpretationRootNamespace) {
  ASSERT_OK_AND_ASSIGN(auto generator,
                       NamespaceGenerator::Create("com.example"));
  std::vector<std::string> qualified_ident = {".foo", "Bar"};
  std::vector<std::pair<std::string, int>> candidates;
  generator.GenerateCandidates(
      qualified_ident, [&](absl::string_view candidate, int segment_index) {
        candidates.push_back(std::pair(std::string(candidate), segment_index));
        return true;
      });
  EXPECT_THAT(candidates, ElementsAre(Pair("foo.Bar", 1), Pair("foo", 0)));
}

}  // namespace
}  // namespace cel::checker_internal
