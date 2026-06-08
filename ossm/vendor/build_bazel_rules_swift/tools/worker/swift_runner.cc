// Copyright 2019 The Bazel Authors. All rights reserved.
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

#include "tools/worker/swift_runner.h"

#include <filesystem>
#include <fstream>

#include "tools/common/bazel_substitutions.h"
#include "tools/common/process.h"
#include "tools/common/temp_file.h"
#include "tools/worker/output_file_map.h"

bool ArgumentEnablesWMO(const std::string &arg) {
  return arg == "-wmo" || arg == "-whole-module-optimization" ||
         arg == "-force-single-frontend-invocation";
}

namespace {

// Creates a temporary file and writes the given arguments to it, one per line.
static std::unique_ptr<TempFile> WriteResponseFile(
    const std::vector<std::string> &args) {
  auto response_file = TempFile::Create("swiftc_params.XXXXXX");
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

// If `str` starts with `prefix`, `str` is mutated to remove `prefix` and the
// function returns true. Otherwise, `str` is left unmodified and the function
// returns `false`.
static bool StripPrefix(const std::string &prefix, std::string &str) {
  if (str.find(prefix) != 0) {
    return false;
  }
  str.erase(0, prefix.size());
  return true;
}

}  // namespace

SwiftRunner::SwiftRunner(const std::vector<std::string> &args,
                         std::string index_import_path, bool force_response_file)
    : job_env_(GetCurrentEnvironment()),
      index_import_path_(index_import_path),
      force_response_file_(force_response_file),
      is_dump_ast_(false),
      file_prefix_pwd_is_dot_(false) {
  args_ = ProcessArguments(args);
}

int SwiftRunner::Run(std::ostream *stderr_stream, bool stdout_to_stderr) {
  int exit_code = RunSubProcess(
      args_, &job_env_, stderr_stream, stdout_to_stderr);

  if (exit_code != 0) {
    return exit_code;
  }

  if (!generated_header_rewriter_path_.empty()) {
#if __APPLE__
    // Skip the `xcrun` argument that's added when running on Apple platforms.
    int initial_args_to_skip = 1;
#else
    int initial_args_to_skip = 0;
#endif

    std::vector<std::string> rewriter_args;
    rewriter_args.reserve(args_.size() + 2 - initial_args_to_skip);
    rewriter_args.push_back(generated_header_rewriter_path_);
    rewriter_args.push_back("--");
    rewriter_args.insert(rewriter_args.end(),
                         args_.begin() + initial_args_to_skip, args_.end());

    exit_code = RunSubProcess(
        rewriter_args, /*env=*/nullptr, stderr_stream, stdout_to_stderr);
  }

  auto enable_global_index_store = global_index_store_import_path_ != "";
  if (enable_global_index_store) {
    if (index_import_path_.empty()) {
      (*stderr_stream) << "Failed to find index-import path from runfiles\n";
      return EXIT_FAILURE;
    }

    OutputFileMap output_file_map;
    output_file_map.ReadFromPath(output_file_map_path_, "", "");

    auto outputs = output_file_map.incremental_outputs();
    std::map<std::string, std::string>::iterator it;

    std::vector<std::string> ii_args;
    ii_args.push_back(index_import_path_);

    if (file_prefix_pwd_is_dot_) {
      ii_args.push_back("-file-prefix-map");
      ii_args.push_back(std::filesystem::current_path().string() + "=.");
    }

    for (it = outputs.begin(); it != outputs.end(); it++) {
      // Need the actual output paths of the compiler - not bazel
      auto output_path = it->first;
      auto file_type = output_path.substr(output_path.find_last_of(".") + 1);
      if (file_type == "o") {
        ii_args.push_back("-import-output-file");
        ii_args.push_back(output_path);
      }
    }

    const std::filesystem::path &exec_root = std::filesystem::current_path();
    // Copy back from the global index store to bazel's index store
    ii_args.push_back((exec_root / global_index_store_import_path_).string());
    ii_args.push_back((exec_root / index_store_path_).string());
    exit_code = RunSubProcess(
        ii_args, /*env=*/nullptr, stderr_stream, /*stdout_to_stderr=*/true);
  }
  return exit_code;
}

// Marker for end of iteration
class StreamIteratorEnd {};

// Basic iterator over an ifstream
class StreamIterator {
 public:
  StreamIterator(std::ifstream &file) : file_{file} { next(); }

  const std::string &operator*() const { return str_; }

  StreamIterator &operator++() {
    next();
    return *this;
  }

  bool operator!=(StreamIteratorEnd) const { return !!file_; }

 private:
  void next() { std::getline(file_, str_); }

  std::ifstream &file_;
  std::string str_;
};

class ArgsFile {
 public:
  ArgsFile(std::ifstream &file) : file_(file) {}

  StreamIterator begin() { return StreamIterator{file_}; }

  StreamIteratorEnd end() { return StreamIteratorEnd{}; }

 private:
  std::ifstream &file_;
};

bool SwiftRunner::ProcessPossibleResponseFile(
    const std::string &arg, std::function<void(const std::string &)> consumer) {
  auto path = arg.substr(1);
  std::ifstream original_file(path);
  ArgsFile args_file(original_file);

  // If we couldn't open it, maybe it's not a file; maybe it's just some other
  // argument that starts with "@" such as "@loader_path/..."
  if (!original_file.good()) {
    consumer(arg);
    return false;
  }

  // Read the file to a vector to prevent double I/O
  auto args = ParseArguments(args_file);

  // If we're forcing response files, process and send the arguments from this
  // file directly to the consumer; they'll all get written to the same response
  // file at the end of processing all the arguments.
  if (force_response_file_) {
    for (auto it = args.begin(); it != args.end(); ++it) {
      // Arguments in response files might be quoted/escaped, so we need to
      // unescape them ourselves.
      ProcessArgument(it, Unescape(*it), consumer);
    }
    return true;
  }

  // Otherwise, open the file, process the arguments, and rewrite it if any of
  // them have changed.
  bool changed = false;
  std::string arg_from_file;
  std::vector<std::string> new_args;
  for (auto it = args.begin(); it != args.end(); ++it) {
    changed |= ProcessArgument(it, *it, [&](const std::string &processed_arg) {
      new_args.push_back(processed_arg);
    });
  }

  if (changed) {
    auto new_file = WriteResponseFile(new_args);
    consumer("@" + new_file->GetPath());
    temp_files_.push_back(std::move(new_file));
  } else {
    // If none of the arguments changed, just keep the original response file
    // argument.
    consumer(arg);
  }

  return changed;
}

template <typename Iterator>
bool SwiftRunner::ProcessArgument(
    Iterator &itr, const std::string &arg,
    std::function<void(const std::string &)> consumer) {
  bool changed = false;
  if (arg[0] == '@') {
    changed = ProcessPossibleResponseFile(arg, consumer);
  } else {
    std::string new_arg = arg;
    if (StripPrefix("-Xwrapped-swift=", new_arg)) {
      if (new_arg == "-debug-prefix-pwd-is-dot") {
        // Replace the $PWD with . to make the paths relative to the workspace
        // without breaking hermiticity.
        consumer("-debug-prefix-map");
        consumer(std::filesystem::current_path().string() + "=.");
        changed = true;
      } else if (new_arg == "-coverage-prefix-pwd-is-dot") {
        // Replace the $PWD with . to make the paths relative to the workspace
        // without breaking hermiticity.
        consumer("-coverage-prefix-map");
        consumer(std::filesystem::current_path().string() + "=.");
        changed = true;
      } else if (new_arg == "-file-prefix-pwd-is-dot") {
        // Replace the $PWD with . to make the paths relative to the workspace
        // without breaking hermiticity.
        consumer("-file-prefix-map");
        consumer(std::filesystem::current_path().string() + "=.");
        changed = true;
      } else if (StripPrefix("-macro-expansion-dir=", new_arg)) {
        changed = true;
        std::filesystem::create_directories(new_arg);
#if __APPLE__
        job_env_["TMPDIR"] = new_arg;
#else
        // TEMPDIR is read by C++ but not Swift. Swift requires the temprorary
        // directory to be an absolute path and otherwise fails (or ignores it
        // silently on macOS) so we need to set one that Swift does not read.
        // C++ prioritizes TMPDIR over TEMPDIR so we need to wipe out the other
        // one. The downside is that anything else reading TMPDIR will not use
        // the one potentially set by the user.
        job_env_["TEMPDIR"] = new_arg;
        job_env_.erase("TMPDIR");
#endif
      } else if (new_arg == "-ephemeral-module-cache") {
        // Create a temporary directory to hold the module cache, which will be
        // deleted after compilation is finished.
        auto module_cache_dir =
            TempDirectory::Create("swift_module_cache.XXXXXX");
        consumer("-module-cache-path");
        consumer(module_cache_dir->GetPath());
        temp_directories_.push_back(std::move(module_cache_dir));
        changed = true;
      } else if (StripPrefix("-generated-header-rewriter=", new_arg)) {
        changed = true;
      } else if (StripPrefix("-bazel-target-label=", new_arg)) {
        changed = true;
      } else if (StripPrefix("-global-index-store-import-path=", new_arg)) {
        changed = true;
      } else {
        // TODO(allevato): Report that an unknown wrapper arg was found and give
        // the caller a way to exit gracefully.
        changed = true;
      }
    } else {
      // Process default arguments
      if (arg == "-index-store-path") {
        consumer("-index-store-path");
        ++itr;

        // If there was a global index store set, pass that to swiftc.
        // Otherwise, pass the users. We later copy index data onto the users.
        if (global_index_store_import_path_ != "") {
          new_arg = global_index_store_import_path_;
        } else {
          new_arg = index_store_path_;
        }
        changed = true;
      } else if (arg == "-output-file-map") {
        // Save the output file map to the value proceeding
        // `-output-file-map`
        consumer("-output-file-map");
        ++itr;
        new_arg = output_file_map_path_;
        changed = true;
      } else if (is_dump_ast_ && ArgumentEnablesWMO(arg)) {
        // WMO is invalid for -dump-ast,
        // so omit the argument that enables WMO
        return true;  // return to avoid consuming the arg
      }

      // Apply any other text substitutions needed in the argument (i.e., for
      // Apple toolchains).
      //
      // Bazel doesn't quote arguments in multi-line params files, so we need
      // to ensure that our defensive quoting kicks in if an argument contains
      // a space, even if no other changes would have been made.
      changed = bazel_placeholder_substitutions_.Apply(new_arg) ||
                new_arg.find_first_of(' ') != std::string::npos;
      consumer(new_arg);
    }
  }

  return changed;
}

template <typename Iterator>
std::vector<std::string> SwiftRunner::ParseArguments(Iterator itr) {
  std::vector<std::string> out_args;
  for (auto it = itr.begin(); it != itr.end(); ++it) {
    auto arg = *it;
    out_args.push_back(arg);

    if (StripPrefix("-Xwrapped-swift=", arg)) {
      if (StripPrefix("-global-index-store-import-path=", arg)) {
        global_index_store_import_path_ = arg;
      } else if (StripPrefix("-generated-header-rewriter=", arg)) {
        generated_header_rewriter_path_ = arg;
      } else if (StripPrefix("-bazel-target-label=", arg)) {
        target_label_ = arg;
      } else if (arg == "-file-prefix-pwd-is-dot") {
        file_prefix_pwd_is_dot_ = true;
      }
    } else {
      if (arg == "-output-file-map") {
        ++it;
        arg = *it;
        output_file_map_path_ = arg;
        out_args.push_back(arg);
      } else if (arg == "-index-store-path") {
        ++it;
        arg = *it;
        index_store_path_ = arg;
        out_args.push_back(arg);
      } else if (arg == "-dump-ast") {
        is_dump_ast_ = true;
      }
    }
  }
  return out_args;
}

std::vector<std::string> SwiftRunner::ProcessArguments(
    const std::vector<std::string> &args) {
  std::vector<std::string> new_args;
  std::vector<std::string> response_file_args;

#if __APPLE__
  // On Apple platforms, inject `/usr/bin/xcrun` in front of our command
  // invocation.
  new_args.push_back("/usr/bin/xcrun");
#endif

  // The tool is assumed to be the first argument. Push it directly.
  auto parsed_args = ParseArguments(args);

  auto it = parsed_args.begin();
  new_args.push_back(*it++);

  // If we're forcing response files, push the remaining processed args onto a
  // different vector that we write out below. If not, push them directly onto
  // the vector being returned.
  auto &args_destination = force_response_file_ ? response_file_args : new_args;
  while (it != parsed_args.end()) {
    ProcessArgument(it, *it, [&](const std::string &arg) {
      args_destination.push_back(arg);
    });
    ++it;
  }

  if (force_response_file_) {
    // Write the processed args to the response file, and push the path to that
    // file (preceded by '@') onto the arg list being returned.
    auto new_file = WriteResponseFile(response_file_args);
    new_args.push_back("@" + new_file->GetPath());
    temp_files_.push_back(std::move(new_file));
  }

  return new_args;
}
