// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fuzzing/replay/test_replayer.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "fuzzing/replay/file_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fuzzing {

namespace {

std::function<int(const uint8_t*, size_t)> CollectTestsCallback(
    std::vector<std::string>* collected_tests) {
  return [collected_tests](const uint8_t* data, size_t size) {
    collected_tests->push_back(
        std::string(reinterpret_cast<const char*>(data), size));
    return 0;
  };
}

TEST(TestReplayerTest, ReplaysFileSuccessfully) {
  const std::string test_file =
      absl::StrCat(getenv("TEST_TMPDIR"), "/single-test-file");
  ASSERT_TRUE(SetFileContents(test_file, "foo").ok());

  std::vector<std::string> collected_tests;
  TestReplayer test_replayer(CollectTestsCallback(&collected_tests),
                             /*max_test_file_size=*/1024);
  EXPECT_TRUE(test_replayer.ReplayTests(test_file).ok());
  EXPECT_THAT(collected_tests, testing::UnorderedElementsAre("foo"));
}

TEST(TestReplayerTest, ReplaysEmptyDirectorySuccessfully) {
  const std::string test_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/empty-dir");
  ASSERT_EQ(mkdir(test_dir.c_str(), 0755), 0);

  std::vector<std::string> collected_tests;
  TestReplayer test_replayer(CollectTestsCallback(&collected_tests),
                             /*max_test_file_size=*/1024);
  EXPECT_TRUE(test_replayer.ReplayTests(test_dir).ok());
  EXPECT_THAT(collected_tests, testing::IsEmpty());
}

TEST(TestReplayerTest, ReplaysNonEmptyDirectorySuccessfully) {
  const std::string test_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/non-empty-dir");
  ASSERT_EQ(mkdir(test_dir.c_str(), 0755), 0);
  const std::string child_dir = absl::StrCat(test_dir, "/child");
  ASSERT_EQ(mkdir(child_dir.c_str(), 0755), 0);
  const std::string leaf_dir = absl::StrCat(child_dir, "/leaf");
  ASSERT_EQ(mkdir(leaf_dir.c_str(), 0755), 0);
  ASSERT_TRUE(SetFileContents(absl::StrCat(test_dir, "/a"), "foo").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(child_dir, "/b"), "bar").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(leaf_dir, "/c"), "baz").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(leaf_dir, "/d"), "boo").ok());

  std::vector<std::string> collected_tests;
  TestReplayer test_replayer(CollectTestsCallback(&collected_tests),
                             /*max_test_file_size=*/1024);
  EXPECT_TRUE(test_replayer.ReplayTests(test_dir).ok());
  EXPECT_THAT(collected_tests,
              testing::UnorderedElementsAre("foo", "bar", "baz", "boo"));
}

TEST(TestReplayerTest, FailsOnMissingFile) {
  std::vector<std::string> collected_tests;
  TestReplayer test_replayer(CollectTestsCallback(&collected_tests),
                             /*max_test_file_size=*/1024);
  EXPECT_FALSE(test_replayer.ReplayTests("missing_path").ok());
  EXPECT_THAT(collected_tests, testing::IsEmpty());
}

}  // namespace

}  // namespace fuzzing
