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

#include "fuzzing/replay/file_util.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fuzzing {

namespace {

std::function<void(absl::string_view, const struct stat&)> CollectPathsCallback(
    std::vector<std::string>* collected_paths) {
  return [collected_paths](absl::string_view path, const struct stat&) {
    absl::FPrintF(stderr, "Collected path: %s\n", path);
    collected_paths->push_back(std::string(path));
  };
}

TEST(YieldFilesTest, ReturnsEmptyResultsOnEmptyDir) {
  const std::string root_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/empty-root");
  ASSERT_EQ(mkdir(root_dir.c_str(), 0755), 0);

  std::vector<std::string> collected_paths;
  const absl::Status status =
      YieldFiles(root_dir, CollectPathsCallback(&collected_paths));
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(collected_paths, testing::IsEmpty());
}

TEST(YieldFilesTest, ReturnsErrorOnMissingDir) {
  const std::string missing_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/missing");
  std::vector<std::string> collected_paths;
  const absl::Status status =
      YieldFiles(missing_dir, CollectPathsCallback(&collected_paths));
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.message(), testing::HasSubstr("could not stat"));
}

TEST(YieldFilesTest, YieldsTopLevelFiles) {
  const std::string root_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/top-level-root");
  ASSERT_EQ(mkdir(root_dir.c_str(), 0755), 0);
  ASSERT_TRUE(SetFileContents(absl::StrCat(root_dir, "/a"), "foo").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(root_dir, "/b"), "bar").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(root_dir, "/c"), "baz").ok());

  std::vector<std::string> collected_paths;
  const absl::Status status =
      YieldFiles(root_dir, CollectPathsCallback(&collected_paths));
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(collected_paths, testing::SizeIs(3));
}

TEST(YieldFilesTest, YieldsDeepFiles) {
  const std::string root_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/deep-root");
  ASSERT_EQ(mkdir(root_dir.c_str(), 0755), 0);
  const std::string child_dir = absl::StrCat(root_dir, "/child");
  ASSERT_EQ(mkdir(child_dir.c_str(), 0755), 0);
  const std::string leaf_dir = absl::StrCat(child_dir, "/leaf");
  ASSERT_EQ(mkdir(leaf_dir.c_str(), 0755), 0);
  ASSERT_TRUE(SetFileContents(absl::StrCat(root_dir, "/a"), "foo").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(child_dir, "/b"), "bar").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(leaf_dir, "/c"), "baz").ok());
  ASSERT_TRUE(SetFileContents(absl::StrCat(leaf_dir, "/d"), "boo").ok());

  std::vector<std::string> collected_paths;
  const absl::Status status =
      YieldFiles(root_dir, CollectPathsCallback(&collected_paths));
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(collected_paths, testing::SizeIs(4));
}

TEST(YieldFilesTest, YieldsHiddenFilesAndDirs) {
  const std::string root_dir =
      absl::StrCat(getenv("TEST_TMPDIR"), "/dir-with-hidden");
  ASSERT_EQ(mkdir(root_dir.c_str(), 0755), 0);
  ASSERT_TRUE(SetFileContents(absl::StrCat(root_dir, "/.a"), "foo").ok());
  const std::string child_dir = absl::StrCat(root_dir, "/.hidden");
  ASSERT_EQ(mkdir(child_dir.c_str(), 0755), 0);
  ASSERT_TRUE(SetFileContents(absl::StrCat(child_dir, "/b"), "bar").ok());

  std::vector<std::string> collected_paths;
  const absl::Status status =
      YieldFiles(root_dir, CollectPathsCallback(&collected_paths));
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(collected_paths, testing::SizeIs(2));
}

}  // namespace

}  // namespace fuzzing
