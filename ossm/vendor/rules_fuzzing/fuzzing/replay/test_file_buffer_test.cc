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

#include "fuzzing/replay/test_file_buffer.h"

#include <cstdlib>
#include <string>

#include "absl/strings/str_cat.h"
#include "fuzzing/replay/file_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fuzzing {

namespace {

TEST(TestFileBufferTest, EmptyBuffer) {
  TestFileBuffer buffer(1024);
  EXPECT_THAT(buffer.last_test(), testing::IsEmpty());
}

TEST(TestFileBufferTest, ReadsFileSuccessfully) {
  const std::string test_file =
      absl::StrCat(getenv("TEST_TMPDIR"), "/successful_test.txt");
  ASSERT_TRUE(SetFileContents(test_file, "123456789").ok());

  TestFileBuffer buffer(1024);
  const absl::Status status = buffer.ReadFile(test_file);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(buffer.last_test(), "123456789");
}

TEST(TestFileBufferTest, FailsOnMissingFile) {
  TestFileBuffer buffer(1024);
  const absl::Status status = buffer.ReadFile("missing_path");
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(buffer.last_test(), testing::IsEmpty());
}

TEST(TestFileBufferTest, TruncatesTooLargeFile) {
  const std::string test_file =
      absl::StrCat(getenv("TEST_TMPDIR"), "/truncated_test.txt");
  ASSERT_TRUE(SetFileContents(test_file, "123456789").ok());

  TestFileBuffer buffer(4);
  const absl::Status status = buffer.ReadFile(test_file);
  EXPECT_EQ(status.code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(buffer.last_test(), "1234");
}

TEST(TestFileBufferTest, OverridesLastTest) {
  const std::string first_test_file =
      absl::StrCat(getenv("TEST_TMPDIR"), "/override_test_first.txt");
  ASSERT_TRUE(SetFileContents(first_test_file, "123456789").ok());
  const std::string second_test_file =
      absl::StrCat(getenv("TEST_TMPDIR"), "/override_test_second.txt");
  ASSERT_TRUE(SetFileContents(second_test_file, "ABCDEF").ok());

  TestFileBuffer buffer(1024);
  EXPECT_TRUE(buffer.ReadFile(first_test_file).ok());
  EXPECT_EQ(buffer.last_test(), "123456789");
  EXPECT_TRUE(buffer.ReadFile(second_test_file).ok());
  EXPECT_EQ(buffer.last_test(), "ABCDEF");
  EXPECT_FALSE(buffer.ReadFile("third_test_file_missing").ok());
  EXPECT_THAT(buffer.last_test(), testing::IsEmpty());
}

}  // namespace

}  // namespace fuzzing
