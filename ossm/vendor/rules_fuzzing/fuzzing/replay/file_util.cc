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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <string>

#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fuzzing/replay/status_util.h"

namespace fuzzing {

namespace {

absl::Status TraverseDirectory(
    absl::string_view path,
    absl::FunctionRef<void(absl::string_view, const struct stat&)> callback) {
  DIR* dir = opendir(std::string(path).c_str());
  if (!dir) {
    return ErrnoStatus(absl::StrCat("could not open directory ", path), errno);
  }
  absl::Status status = absl::OkStatus();
  while (true) {
    errno = 0;
    struct dirent* entry = readdir(dir);
    if (!entry) {
      if (errno) {
        status.Update(ErrnoStatus(
            absl::StrCat("could not complete directory traversal for ", path),
            errno));
      }
      break;
    }
    auto entry_name = absl::string_view(entry->d_name);
    if (entry_name == "." || entry_name == "..") {
      continue;
    }
    const std::string entry_path = absl::StrCat(path, "/", entry_name);
    status.Update(YieldFiles(entry_path, callback));
  }
  closedir(dir);
  return status;
}

}  // namespace

absl::Status YieldFiles(
    absl::string_view path,
    absl::FunctionRef<void(absl::string_view, const struct stat&)> callback) {
  struct stat path_stat;
  if (stat(std::string(path).c_str(), &path_stat) < 0) {
    return ErrnoStatus(absl::StrCat("could not stat ", path), errno);
  }
  if (S_ISDIR(path_stat.st_mode)) {
    return TraverseDirectory(path, callback);
  }
  callback(path, path_stat);
  return absl::OkStatus();
}

absl::Status SetFileContents(absl::string_view path,
                             absl::string_view contents) {
  FILE* f = fopen(std::string(path).c_str(), "w");
  if (!f) {
    return ErrnoStatus("could not open file", errno);
  }
  const size_t result = fwrite(contents.data(), 1, contents.size(), f);
  fclose(f);
  if (result < contents.size()) {
    return absl::UnknownError("could not write file contents");
  }
  return absl::OkStatus();
}

}  // namespace fuzzing
