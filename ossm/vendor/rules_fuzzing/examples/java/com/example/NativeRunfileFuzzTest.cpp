// Copyright 2021 Google LLC
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

#include "NativeRunfileFuzzTest.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "tools/cpp/runfiles/runfiles.h"

JNIEXPORT void JNICALL
Java_com_example_NativeRunfileFuzzTest_loadCppRunfile(JNIEnv *env,
                                                          jobject o) {
  using ::bazel::tools::cpp::runfiles::Runfiles;
  std::string error;
  auto runfiles = std::unique_ptr<Runfiles>(Runfiles::Create("", &error));
  if (runfiles == nullptr) {
    std::cerr << error;
    abort();
  }
  std::string path =
      runfiles->Rlocation("rules_fuzzing/examples/java/corpus_1.txt");
  if (path.empty()) abort();
  std::ifstream in(path);
  if (!in.good()) abort();
}
