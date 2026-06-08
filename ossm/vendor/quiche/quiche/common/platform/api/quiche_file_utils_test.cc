#include "quiche/common/platform/api/quiche_file_utils.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche {
namespace test {
namespace {

using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

TEST(QuicheFileUtilsTest, ReadFileContents) {
  std::string path = absl::StrCat(QuicheGetCommonSourcePath(),
                                  "/platform/api/testdir/testfile");
  std::optional<std::string> contents = ReadFileContents(path);
  ASSERT_TRUE(contents.has_value());
  EXPECT_EQ(*contents, "This is a test file.");
}

TEST(QuicheFileUtilsTest, ReadFileContentsFileNotFound) {
  std::string path =
      absl::StrCat(QuicheGetCommonSourcePath(),
                   "/platform/api/testdir/file-that-does-not-exist");
  std::optional<std::string> contents = ReadFileContents(path);
  EXPECT_FALSE(contents.has_value());
}

TEST(QuicheFileUtilsTest, EnumerateDirectory) {
  std::string path =
      absl::StrCat(QuicheGetCommonSourcePath(), "/platform/api/testdir");
  std::vector<std::string> dirs;
  std::vector<std::string> files;
  bool success = EnumerateDirectory(path, dirs, files);
  EXPECT_TRUE(success);
  EXPECT_THAT(files, UnorderedElementsAre("testfile", "README.md"));
  EXPECT_THAT(dirs, UnorderedElementsAre("a"));
}

TEST(QuicheFileUtilsTest, EnumerateDirectoryNoSuchDirectory) {
  std::string path = absl::StrCat(QuicheGetCommonSourcePath(),
                                  "/platform/api/testdir/no-such-directory");
  std::vector<std::string> dirs;
  std::vector<std::string> files;
  bool success = EnumerateDirectory(path, dirs, files);
  EXPECT_FALSE(success);
}

TEST(QuicheFileUtilsTest, EnumerateDirectoryNotADirectory) {
  std::string path = absl::StrCat(QuicheGetCommonSourcePath(),
                                  "/platform/api/testdir/testfile");
  std::vector<std::string> dirs;
  std::vector<std::string> files;
  bool success = EnumerateDirectory(path, dirs, files);
  EXPECT_FALSE(success);
}

TEST(QuicheFileUtilsTest, EnumerateDirectoryRecursively) {
  std::vector<std::string> expected_paths = {"a/b/c/d/e", "a/subdir/testfile",
                                             "a/z", "testfile", "README.md"};

  std::string root_path =
      absl::StrCat(QuicheGetCommonSourcePath(), "/platform/api/testdir");
  for (std::string& path : expected_paths) {
    // For Windows, use Windows path separators.
    if (JoinPath("a", "b") == "a\\b") {
      absl::c_replace(path, '/', '\\');
    }

    path = JoinPath(root_path, path);
  }

  std::vector<std::string> files;
  bool success = EnumerateDirectoryRecursively(root_path, files);
  EXPECT_TRUE(success);
  EXPECT_THAT(files, UnorderedElementsAreArray(expected_paths));
}

}  // namespace
}  // namespace test
}  // namespace quiche
