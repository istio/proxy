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

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "tools/cpp/runfiles/runfiles.h"
#include "tools/worker/compile_with_worker.h"
#include "tools/worker/compile_without_worker.h"

using bazel::tools::cpp::runfiles::Runfiles;

int main(int argc, char *argv[]) {
  std::string index_import_path;
  #ifdef BAZEL_CURRENT_REPOSITORY
    std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0], BAZEL_CURRENT_REPOSITORY));
  #else
    std::unique_ptr<Runfiles> runfiles(Runfiles::Create(argv[0]));
  #endif  // BAZEL_CURRENT_REPOSITORY
  if (runfiles != nullptr) {
    // We silently ignore errors here, we will report an error later if this
    // path is accessed
    index_import_path =
      runfiles->Rlocation("build_bazel_rules_swift_index_import/index-import");
  }

  auto args = std::vector<std::string>(argv + 1, argv + argc);

  // When Bazel invokes a tool in persistent worker mode, it includes the flag
  // "--persistent_worker" on the command line (typically the first argument,
  // but we don't want to rely on that). Since this "worker" tool also supports
  // a non-worker mode, we can detect the mode based on the presence of this
  // flag.
  auto persistent_worker_it =
      std::find(args.begin(), args.end(), "--persistent_worker");
  if (persistent_worker_it == args.end()) {
    return CompileWithoutWorker(args, index_import_path);
  }

  // Remove the special flag before starting the worker processing loop.
  args.erase(persistent_worker_it);
  return CompileWithWorker(args, index_import_path);
}
