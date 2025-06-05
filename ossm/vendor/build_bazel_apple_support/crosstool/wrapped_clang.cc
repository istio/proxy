// Copyright 2017 The Bazel Authors. All rights reserved.
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
//
// wrapped_clang.cc: Pass args to 'xcrun clang' and zip dsym files.
//
// wrapped_clang passes its args to clang, but also supports a separate set of
// invocations to generate dSYM files.  If "DSYM_HINT" flags are passed in, they
// are used to construct that separate set of invocations (instead of being
// passed to clang).
// The following "DSYM_HINT" flags control dsym generation.  If any one if these
// are passed in, then they all must be passed in.
// "DSYM_HINT_LINKED_BINARY": Workspace-relative path to binary output of the
//    link action generating the dsym file.
// "DSYM_HINT_DSYM_PATH": Workspace-relative path to dSYM dwarf file.

#include <libgen.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

extern char **environ;

namespace {

constexpr char kAddASTPathPrefix[] = "-Wl,-add_ast_path,";

// Returns the base name of the given filepath. For example, given
// /foo/bar/baz.txt, returns 'baz.txt'.
const char *Basename(const char *filepath) {
  const char *base = strrchr(filepath, '/');
  return base ? (base + 1) : filepath;
}

// Unescape and unquote an argument read from a line of a response file.
static std::string Unescape(const std::string &arg) {
  std::string result;
  auto length = arg.size();
  for (size_t i = 0; i < length; ++i) {
    auto ch = arg[i];

    // If it's a backslash, consume it and append the character that follows.
    if (ch == '\\' && i + 1 < length) {
      ++i;
      result.push_back(arg[i]);
      continue;
    }

    // If it's a quote, process everything up to the matching quote, unescaping
    // backslashed characters as needed.
    if (ch == '"' || ch == '\'') {
      auto quote = ch;
      ++i;
      while (i != length && arg[i] != quote) {
        if (arg[i] == '\\' && i + 1 < length) {
          ++i;
        }
        result.push_back(arg[i]);
        ++i;
      }
      if (i == length) {
        break;
      }
      continue;
    }

    // It's a regular character.
    result.push_back(ch);
  }

  return result;
}

// Converts an array of string arguments to char *arguments.
// The first arg is reduced to its basename as per execve conventions.
// Note that the lifetime of the char* arguments in the returned array
// are controlled by the lifetime of the strings in args.
std::vector<const char *> ConvertToCArgs(const std::vector<std::string> &args) {
  std::vector<const char *> c_args;
  c_args.push_back(Basename(args[0].c_str()));
  for (int i = 1; i < args.size(); i++) {
    c_args.push_back(args[i].c_str());
  }
  c_args.push_back(nullptr);
  return c_args;
}

// Spawns a subprocess for given arguments args. The first argument is used
// for the executable path.
bool RunSubProcess(const std::vector<std::string> &args) {
  std::vector<const char *> exec_argv = ConvertToCArgs(args);
  pid_t pid;
  int status = posix_spawn(&pid, args[0].c_str(), nullptr, nullptr,
                           const_cast<char **>(exec_argv.data()), environ);
  if (status == 0) {
    int wait_status;
    do {
      wait_status = waitpid(pid, &status, 0);
    } while ((wait_status == -1) && (errno == EINTR));
    if (wait_status < 0) {
      std::cerr << "Error waiting on child process '" << args[0] << "'. "
                << strerror(errno) << "\n";
      return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      std::cerr << "Error in child process '" << args[0] << "'. "
                << WEXITSTATUS(status) << "\n";
      return false;
    } else if (WIFSIGNALED(status)) {
      std::cerr << "Error in child process '" << args[0] << "'. "
                << WTERMSIG(status) << "\n";
      return false;
    }
  } else {
    std::cerr << "Error forking process '" << args[0] << "'. "
              << strerror(status) << "\n";
    return false;
  }

  return true;
}

// Finds and replaces all instances of oldsub with newsub, in-place on str.
void FindAndReplace(const std::string &oldsub, const std::string &newsub,
                    std::string *str) {
  int start = 0;
  while ((start = str->find(oldsub, start)) != std::string::npos) {
    str->replace(start, oldsub.length(), newsub);
    start += newsub.length();
  }
}

// If arg is of the classic flag form "foo=bar", and flagname is 'foo', sets
// str to point to a new std::string 'bar' and returns true.
// Otherwise, returns false.
bool SetArgIfFlagPresent(const std::string &arg, const std::string &flagname,
                         std::string *str) {
  std::string prefix_string = flagname + "=";
  if (arg.compare(0, prefix_string.length(), prefix_string) == 0) {
    *str = arg.substr(prefix_string.length());
    return true;
  }
  return false;
}

// Returns the DEVELOPER_DIR environment variable in the current process
// environment. Aborts if this variable is unset.
std::string GetMandatoryEnvVar(const std::string &var_name) {
  char *env_value = getenv(var_name.c_str());
  if (env_value == nullptr) {
    std::cerr << "Error: " << var_name << " not set.\n";
    exit(EXIT_FAILURE);
  }
  return env_value;
}

// Returns true if `str` starts with the specified `prefix`.
bool StartsWith(const std::string &str, const std::string &prefix) {
  return str.compare(0, prefix.size(), prefix) == 0;
}

// If *`str` begins `prefix`, strip it out and return true.
// Otherwise leave *`str` unchanged and return false.
bool StripPrefixStringIfPresent(std::string *str, const std::string &prefix) {
  if (StartsWith(*str, prefix)) {
    *str = str->substr(prefix.size());
    return true;
  }
  return false;
}

// An RAII temporary file.
class TempFile {
 public:
  // Create a new temporary file using the given path template string (the same
  // form used by `mkstemp`). The file will automatically be deleted when the
  // object goes out of scope.
  static std::unique_ptr<TempFile> Create(const std::string &path_template) {
    const char *tmpDir = getenv("TMPDIR");
    if (!tmpDir) {
      tmpDir = "/tmp";
    }
    size_t size = strlen(tmpDir) + path_template.size() + 2;
    std::unique_ptr<char[]> path(new char[size]);
    snprintf(path.get(), size, "%s/%s", tmpDir, path_template.c_str());

    if (mkstemp(path.get()) == -1) {
      std::cerr << "Failed to create temporary file '" << path.get()
                << "': " << strerror(errno) << "\n";
      return nullptr;
    }
    return std::unique_ptr<TempFile>(new TempFile(path.get()));
  }

  // Explicitly make TempFile non-copyable and movable.
  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;
  TempFile(TempFile &&) = default;
  TempFile &operator=(TempFile &&) = default;

  ~TempFile() { remove(path_.c_str()); }

  // Gets the path to the temporary file.
  std::string GetPath() const { return path_; }

 private:
  explicit TempFile(const std::string &path) : path_(path) {}

  std::string path_;
};

static std::unique_ptr<TempFile> WriteResponseFile(
    const std::vector<std::string> &args) {
  auto response_file = TempFile::Create("wrapped_clang_params.XXXXXX");
  std::ofstream response_file_stream(response_file->GetPath());

  for (const auto &arg : args) {
    // When Clang/Swift write out a response file to communicate from driver to
    // frontend, they just quote every argument to be safe; we duplicate that
    // instead of trying to be "smarter" and only quoting when necessary.
    response_file_stream << '"';
    for (auto ch : arg) {
      if (ch == '"' || ch == '\\') {
        response_file_stream << '\\';
      }
      response_file_stream << ch;
    }
    response_file_stream << "\"\n";
  }

  response_file_stream.close();
  return response_file;
}

void ProcessArgument(const std::string arg, const std::string developer_dir,
                     const std::string sdk_root, const std::string cwd,
                     bool relative_ast_path, std::string &linked_binary,
                     std::string &dsym_path, std::string toolchain_path,
                     std::function<void(const std::string &)> consumer);

bool ProcessResponseFile(const std::string arg, const std::string developer_dir,
                         const std::string sdk_root, const std::string cwd,
                         bool relative_ast_path, std::string &linked_binary,
                         std::string &dsym_path, std::string toolchain_path,
                         std::function<void(const std::string &)> consumer) {
  auto path = arg.substr(1);
  std::ifstream original_file(path);
  // Ignore non-file args such as '@loader_path/...'
  if (!original_file.good()) {
    return false;
  }

  std::string arg_from_file;
  while (std::getline(original_file, arg_from_file)) {
    // Arguments in response files might be quoted/escaped, so we need to
    // unescape them ourselves.
    ProcessArgument(Unescape(arg_from_file), developer_dir, sdk_root, cwd,
                    relative_ast_path, linked_binary, dsym_path,
                    toolchain_path, consumer);
  }

  return true;
}

std::string GetCurrentDirectory() {
  // Passing null,0 causes getcwd to allocate the buffer of the correct size.
  char *buffer = getcwd(nullptr, 0);
  std::string cwd(buffer);
  free(buffer);
  return cwd;
}

std::string exec(std::string cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
      std::cerr << "Error: failed to open pipe to '" << cmd << "'" << std::endl;
      exit(EXIT_FAILURE);
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }
    return result;
}

std::string GetToolchainPath(const std::string &toolchain_id) {
  // NOTE: This requires all toolchains to contain a 'clang' executable. This
  // is true today for custom Swift toolchains, but could change in the future.
  std::string output = exec("xcrun --find clang --toolchain " + toolchain_id);

  if (output.empty()) {
    std::cerr << "Error: TOOLCHAINS was set to '" << toolchain_id
      << "' but no toolchain with that ID was found" << std::endl;
    exit(EXIT_FAILURE);
  } else if (output.find("XcodeDefault.xctoolchain") != std::string::npos) {
    // NOTE: Ideally xcrun would fail if the toolchain we asked for didn't exist
    // but it falls back to the DEVELOPER_DIR instead, so we have to check the
    // output ourselves.
    std::cerr << "Error: TOOLCHAINS was set to '" << toolchain_id
      << "' but the default toolchain was found, that likely means a matching "
      << "toolchain isn't installed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::filesystem::path toolchain_path(output);
  // Remove usr/bin/clang components to get the root of the custom toolchain
  return toolchain_path.parent_path().parent_path().parent_path();
}

void ProcessArgument(const std::string arg, const std::string developer_dir,
                     const std::string sdk_root, const std::string cwd,
                     bool relative_ast_path, std::string &linked_binary,
                     std::string &dsym_path, std::string toolchain_path,
                     std::function<void(const std::string &)> consumer) {
  auto new_arg = arg;
  if (arg[0] == '@') {
    if (ProcessResponseFile(arg, developer_dir, sdk_root, cwd,
                            relative_ast_path, linked_binary, dsym_path,
                            toolchain_path, consumer)) {
      return;
    }
  }

  if (SetArgIfFlagPresent(arg, "DSYM_HINT_LINKED_BINARY", &linked_binary)) {
    return;
  }
  if (SetArgIfFlagPresent(arg, "DSYM_HINT_DSYM_PATH", &dsym_path)) {
    return;
  }

  FindAndReplace("__BAZEL_EXECUTION_ROOT__", cwd, &new_arg);
  FindAndReplace("__BAZEL_XCODE_DEVELOPER_DIR__", developer_dir, &new_arg);
  FindAndReplace("__BAZEL_XCODE_SDKROOT__", sdk_root, &new_arg);
  if (!toolchain_path.empty()) {
    FindAndReplace("__BAZEL_CUSTOM_XCODE_TOOLCHAIN_PATH__", toolchain_path, &new_arg);
  }

  // Make the `add_ast_path` options used to embed Swift module references
  // absolute to enable Swift debugging without dSYMs: see
  // https://forums.swift.org/t/improving-swift-lldb-support-for-path-remappings/22694
  if (!relative_ast_path &&
      StripPrefixStringIfPresent(&new_arg, kAddASTPathPrefix)) {
    // Only modify relative paths.
    if (!StartsWith(arg, "/")) {
      new_arg = std::string(kAddASTPathPrefix) + cwd + "/" + new_arg;
    } else {
      new_arg = std::string(kAddASTPathPrefix) + new_arg;
    }
  }

  consumer(new_arg);
}

}  // namespace

int main(int argc, char *argv[]) {
  std::string tool_name;

  std::string binary_name = Basename(argv[0]);
  if (binary_name == "wrapped_clang_pp") {
    tool_name = "clang++";
  } else if (binary_name == "wrapped_clang") {
    tool_name = "clang";
  } else {
    std::cerr << "Binary must either be named 'wrapped_clang' or "
                 "'wrapped_clang_pp', not "
              << binary_name << "\n";
    return 1;
  }

  const char *toolchain_id = getenv("TOOLCHAINS");
  std::string toolchain_path = "";
  if (toolchain_id != nullptr) {
    toolchain_path = GetToolchainPath(toolchain_id);
  }

  std::string developer_dir = GetMandatoryEnvVar("DEVELOPER_DIR");
  std::string sdk_root = GetMandatoryEnvVar("SDKROOT");
  std::string linked_binary, dsym_path;

  const std::string cwd = GetCurrentDirectory();
  std::vector<std::string> invocation_args = {"/usr/bin/xcrun", tool_name};
  std::vector<std::string> processed_args = {};

  bool relative_ast_path = getenv("RELATIVE_AST_PATH") != nullptr;
  auto consumer = [&](const std::string &arg) {
    processed_args.push_back(arg);
  };
  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);

    ProcessArgument(arg, developer_dir, sdk_root, cwd, relative_ast_path,
                    linked_binary, dsym_path, toolchain_path, consumer);
  }

  // Special mode that only prints the command. Used for testing.
  if (getenv("__WRAPPED_CLANG_LOG_ONLY")) {
    for (const std::string &arg : invocation_args) std::cout << arg << ' ';
    for (const std::string &arg : processed_args) std::cout << arg << ' ';
    std::cout << "\n";
    return 0;
  }

  auto response_file = WriteResponseFile(processed_args);
  invocation_args.push_back("@" + response_file->GetPath());

  // Check to see if we should postprocess with dsymutil.
  bool postprocess = false;
  if ((!linked_binary.empty()) || (!dsym_path.empty())) {
    if ((linked_binary.empty()) || (dsym_path.empty())) {
      const char *missing_dsym_flag;
      if (linked_binary.empty()) {
        missing_dsym_flag = "DSYM_HINT_LINKED_BINARY";
      } else {
        missing_dsym_flag = "DSYM_HINT_DSYM_PATH";
      }
      std::cerr << "Error in clang wrapper: If any dsym "
                   "hint is defined, then "
                << missing_dsym_flag << " must be defined\n";
      return 1;
    } else {
      postprocess = true;
    }
  }

  if (!RunSubProcess(invocation_args)) {
    return 1;
  }

  if (!postprocess) {
    return 0;
  }

  std::vector<std::string> dsymutil_args = {"/usr/bin/xcrun",
                                            "dsymutil",
                                            linked_binary,
                                            "-o",
                                            dsym_path,
                                            "--flat",
                                            "--no-swiftmodule-timestamp"};
  if (!RunSubProcess(dsymutil_args)) {
    return 1;
  }

  return 0;
}
