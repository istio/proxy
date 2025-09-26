// Copyright 2018 The Bazel Authors. All rights reserved.
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

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_TEMP_FILE_H
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_TEMP_FILE_H

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#endif
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#endif

// An RAII temporary file.
class TempFile {
 public:
  // Explicitly make TempFile non-copyable and movable.
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
  TempFile(TempFile &&) = default;
  TempFile &operator=(TempFile &&) = default;

  // Create a new temporary file using the given path template string (the same
  // form used by `mkstemp`). The file will automatically be deleted when the
  // object goes out of scope.
  static std::unique_ptr<TempFile> Create(const std::string &path_template) {
    std::error_code ec;
    std::filesystem::path temporary{std::filesystem::temp_directory_path(ec) / path_template};
    if (ec)
      return nullptr;

    std::string path = temporary.string();

#if defined(_WIN32)
    if (errno_t error = _mktemp_s(path.data(), path.size() + 1)) {
      std::cerr << "Failed to create temporary file '" << temporary
                << "': " << strerror(error) << "\n";
      return nullptr;
    }
#else
    if (::mkstemp(path.data()) < 0) {
      std::cerr << "Failed to create temporary file '" << temporary
                << "': " << strerror(errno) << "\n";
      return nullptr;
    }
#endif

    return std::unique_ptr<TempFile>(new TempFile(path));
  }

  ~TempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  // Gets the path to the temporary file.
  std::string GetPath() const { return path_; }

 private:
  explicit TempFile(const std::string &path) : path_(path) {}

  std::string path_;
};

// An RAII temporary directory that is recursively deleted.
class TempDirectory {
 public:
  // Explicitly make TempDirectory non-copyable and movable.
  TempDirectory(const TempDirectory &) = delete;
  TempDirectory &operator=(const TempDirectory &) = delete;
  TempDirectory(TempDirectory &&) = default;
  TempDirectory &operator=(TempDirectory &&) = default;

  // Create a new temporary directory using the given path template string (the
  // same form used by `mkdtemp`). The file will automatically be deleted when
  // the object goes out of scope.
  static std::unique_ptr<TempDirectory> Create(
      const std::string &path_template) {
    std::error_code ec;
    std::filesystem::path temporary{std::filesystem::temp_directory_path(ec) / path_template};
    if (ec)
      return nullptr;

    std::string path = temporary.string();

#if defined(_WIN32)
    if (memcmp(path.data() + path.length() - 6, "XXXXXX", 6)) {
      std::cerr << "Failed to create temporary directory '" << temporary
                << "': invalid parameter\n";
      return nullptr;
    }

    auto randname = [](char *buffer) {
      static const char kAlphabet[] =
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01123456789";
      constexpr size_t kAlphabetSize = sizeof(kAlphabet) - 1;

      for (unsigned index = 0; index < 6; ++index)
        buffer[index] = kAlphabet[rand() % kAlphabetSize];
    };

    srand(reinterpret_cast<uintptr_t>(path.data()));
    for (unsigned retry = 256; retry; --retry) {
      randname(path.data() + path.length() - 6);
      if (!_mkdir(path.c_str()))
        break;
    }
#else
    if (::mkdtemp(path.data()) == nullptr) {
      std::cerr << "Failed to create temporary directory '" << temporary
                << "': " << strerror(errno) << "\n";
      return nullptr;
    }
#endif

    return std::unique_ptr<TempDirectory>(new TempDirectory(path));
  }

  ~TempDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  // Gets the path to the temporary directory.
  std::string GetPath() const { return path_; }

 private:
  explicit TempDirectory(const std::string &path) : path_(path) {}

  std::string path_;
};

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_TEMP_FILE_H
