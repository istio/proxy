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

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_COMPILE_WITH_WORKER_H_
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_COMPILE_WITH_WORKER_H_

#include <string>
#include <vector>

// Starts the worker processing loop and listens to stdin for work requests from
// Bazel.
int CompileWithWorker(const std::vector<std::string> &args,
                      std::string index_import_path);

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_WORKER_COMPILE_WITH_WORKER_H_
