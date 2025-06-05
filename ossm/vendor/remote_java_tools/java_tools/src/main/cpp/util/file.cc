// Copyright 2014 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/main/cpp/util/file.h"

#include <limits.h>  // PATH_MAX

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "src/main/cpp/util/errors.h"
#include "src/main/cpp/util/exit_code.h"
#include "src/main/cpp/util/path.h"
#include "src/main/cpp/util/strings.h"

namespace blaze_util {

using std::string;
using std::vector;

bool ReadFrom(file_handle_type handle, string *content, int max_size) {
  static const size_t kReadSize = 4096;  // read 4K chunks
  content->clear();
  char buf[kReadSize];
  // OPT:  This loop generates one spurious read on regular files.
  int error;
  while (int r = ReadFromHandle(
             handle, buf,
             max_size > 0 ? std::min(static_cast<size_t>(max_size), kReadSize)
                          : kReadSize,
             &error)) {
    if (r < 0) {
      if (error == ReadFileResult::INTERRUPTED ||
          error == ReadFileResult::AGAIN) {
        continue;
      }
      return false;
    }
    content->append(buf, r);
    if (max_size > 0) {
      if (max_size > r) {
        max_size -= r;
      } else {
        break;
      }
    }
  }
  return true;
}

bool ReadFrom(file_handle_type handle, void *data, size_t size) {
  static const size_t kReadSize = 4096;  // read 4K chunks
  size_t offset = 0;
  int error;
  while (int r = ReadFromHandle(handle, reinterpret_cast<char *>(data) + offset,
                                std::min(kReadSize, size), &error)) {
    if (r < 0) {
      if (error == ReadFileResult::INTERRUPTED ||
          error == ReadFileResult::AGAIN) {
        continue;
      }
      return false;
    }
    offset += r;
    if (size > static_cast<size_t>(r)) {
      size -= r;
    } else {
      break;
    }
  }
  return true;
}

bool WriteFile(const std::string &content, const std::string &filename,
               unsigned int perm) {
  return WriteFile(content.c_str(), content.size(), filename, perm);
}

bool WriteFile(const std::string &content, const Path &path,
               unsigned int perm) {
  return WriteFile(content.c_str(), content.size(), path, perm);
}

class DirectoryTreeWalker : public DirectoryEntryConsumer {
 public:
  DirectoryTreeWalker(vector<string> *files,
                      _ForEachDirectoryEntry walk_entries)
      : _files(files), _walk_entries(walk_entries) {}

  void Consume(const string &path, bool follow_directory) override {
    if (follow_directory) {
      Walk(path);
    } else {
      _files->push_back(path);
    }
  }

  void Walk(const string &path) { _walk_entries(path, this); }

 private:
  vector<string> *_files;
  _ForEachDirectoryEntry _walk_entries;
};

void GetAllFilesUnder(const string &path, vector<string> *result) {
  _GetAllFilesUnder(path, result, &ForEachDirectoryEntry);
}

void _GetAllFilesUnder(const string &path,
                       vector<string> *result,
                       _ForEachDirectoryEntry walk_entries) {
  DirectoryTreeWalker(result, walk_entries).Walk(path);
}

}  // namespace blaze_util
