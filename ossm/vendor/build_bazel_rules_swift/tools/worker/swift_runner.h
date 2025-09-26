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

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_SWIFT_RUNNER_H_
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_SWIFT_RUNNER_H_

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tools/common/bazel_substitutions.h"
#include "tools/common/temp_file.h"

// Returns true if the given command line argument enables whole-module
// optimization in the compiler.
extern bool ArgumentEnablesWMO(const std::string &arg);

// Handles spawning the Swift compiler driver, making any required substitutions
// of the command line arguments (for example, Bazel's magic Xcode placeholder
// strings).
//
// The first argument in the list passed to the spawner should be the Swift
// tool that should be invoked (for example, "swiftc"). This spawner also
// recognizes special arguments of the form `-Xwrapped-swift=<arg>`. Arguments
// of this form are consumed entirely by this wrapper and are not passed down to
// the Swift tool (however, they may add normal arguments that will be passed).
//
// The following spawner-specific arguments are supported:
//
// -Xwrapped-swift=-debug-prefix-pwd-is-dot
//     When specified, the Swift compiler will be directed to remap the current
//     directory's path to the string "." in debug info. This remapping must be
//     applied here because we do not know the current working directory at
//     analysis time when the argument list is constructed.
//
// -Xwrapped-swift=-file-prefix-pwd-is-dot
//     When specified, the Swift compiler will be directed to remap the current
//     directory's path to the string "." in debug, coverage, and index info.
//     This remapping must be applied here because we do not know the current
//     working directory at analysis time when the argument list is constructed.
//
// -Xwrapped-swift=-ephemeral-module-cache
//     When specified, the spawner will create a new temporary directory, pass
//     that to the Swift compiler using `-module-cache-path`, and then delete
//     the directory afterwards. This should resolve issues where the module
//     cache state is not refreshed correctly in all situations, which
//     sometimes results in hard-to-diagnose crashes in `swiftc`.
class SwiftRunner {
 public:
  // Create a new spawner that launches a Swift tool with the given arguments.
  // The first argument is assumed to be that tool. If force_response_file is
  // true, then the remaining arguments will be unconditionally written into a
  // response file instead of being passed on the command line.
  SwiftRunner(const std::vector<std::string> &args,
              std::string index_import_path,
              bool force_response_file = false);

  // Run the Swift compiler, redirecting stderr to the specified stream. If
  // stdout_to_stderr is true, then stdout is also redirected to that stream.
  int Run(std::ostream *stderr_stream, bool stdout_to_stderr = false);

 private:
  // Processes an argument that looks like it might be a response file (i.e., it
  // begins with '@') and returns true if the argument(s) passed to the consumer
  // were different than "arg").
  //
  // If the argument is not actually a response file (i.e., it begins with '@'
  // but the file cannot be read), then it is passed directly to the consumer
  // and this method returns false. Otherwise, if the response file could be
  // read, this method's behavior depends on a few factors:
  //
  // - If the spawner is forcing response files, then the arguments in this
  //   response file are read and processed and sent directly to the consumer.
  //   In other words, they will be rewritten into that new response file
  //   directly, rather than being kept in their own separate response file.
  //   This is because there is no reason to maintain the original and multiple
  //   response files at this stage of processing. In this case, the function
  //   returns true.
  //
  // - If the spawner is not forcing response files, then the arguments in this
  //   response file are read and processed. If none of the arguments changed,
  //   then this function passes the original response file argument to the
  //   consumer and returns false. If some arguments did change, then they are
  //   written to a new response file, a response file argument pointing to that
  //   file is passed to the consumer, and the method returns true.
  bool ProcessPossibleResponseFile(
      const std::string &arg,
      std::function<void(const std::string &)> consumer);

  // Applies substitutions for a single argument and passes the new arguments
  // (or the original, if no substitution was needed) to the consumer. Returns
  // true if any substitutions were made (that is, if the arguments passed to
  // the consumer were anything different than "arg").
  //
  // This method has file system side effects, creating temporary files and
  // directories as needed for a particular substitution.
  template <typename Iterator>
  bool ProcessArgument(Iterator &itr, const std::string &arg,
                       std::function<void(const std::string &)> consumer);

  // Parses arguments to ivars and returns a vector of strings from the
  // iterator. This method doesn't actually mutate any of the arguments.
  template <typename Iterator>
  std::vector<std::string> ParseArguments(Iterator itr);

  // Applies substitutions to the given command line arguments, returning the
  // results in a new vector.
  std::vector<std::string> ProcessArguments(
      const std::vector<std::string> &args);

  // A mapping of Bazel placeholder strings to the actual paths that should be
  // substituted for them. Supports Xcode resolution on Apple OSes.
  bazel_rules_swift::BazelPlaceholderSubstitutions
      bazel_placeholder_substitutions_;

  // The arguments, post-substitution, passed to the spawner.
  std::vector<std::string> args_;

  // The environment that should be passed to the original job (but not to other
  // jobs spawned by the worker, such as the generated header rewriter or the
  // emit-imports job).
  std::map<std::string, std::string> job_env_;

  // The path to the index-import binary.
  std::string index_import_path_;

  // Temporary files (e.g., rewritten response files) that should be cleaned up
  // after the driver has terminated.
  std::vector<std::unique_ptr<TempFile>> temp_files_;

  // Temporary directories (e.g., ephemeral module cache) that should be cleaned
  // up after the driver has terminated.
  std::vector<std::unique_ptr<TempDirectory>> temp_directories_;

  // Arguments will be unconditionally written into a response file and passed
  // to the tool that way.
  bool force_response_file_;

  // Whether the invocation is being used to dump ast files.
  // This is used to avoid implicitly adding incompatible flags.
  bool is_dump_ast_;

  // Whether `-file-prefix-map PWD=.` is set.
  bool file_prefix_pwd_is_dot_;

  // The path to the generated header rewriter tool, if one is being used for
  // this compilation.
  std::string generated_header_rewriter_path_;

  // The Bazel target label that spawned the worker request, which can be used
  // in custom diagnostic messages printed by the worker.
  std::string target_label_;

  // The path of the output map file
  std::string output_file_map_path_;

  // The index store path argument passed to the runner
  std::string index_store_path_;

  // The path of the global index store  when using
  // swift.use_global_index_store. When set, this is passed to `swiftc` as the
  // `-index-store-path`. After running `swiftc` `index-import` copies relevant
  // index outputs into the `index_store_path` to integrate outputs with Bazel.
  std::string global_index_store_import_path_;
};

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_SWIFT_RUNNER_H_
