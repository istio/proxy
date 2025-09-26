// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "test/utility.h"

namespace proxy_wasm {

std::string TestContext::global_log_;

std::vector<std::string> getWasmEngines() {
  std::vector<std::string> engines = {
#if defined(PROXY_WASM_HOST_ENGINE_V8)
    "v8",
#endif
#if defined(PROXY_WASM_HOST_ENGINE_WAMR)
    "wamr",
#endif
#if defined(PROXY_WASM_HOST_ENGINE_WASMEDGE)
    "wasmedge",
#endif
#if defined(PROXY_WASM_HOST_ENGINE_WASMTIME)
    "wasmtime",
#endif
#if defined(PROXY_WASM_HOST_ENGINE_WAVM)
    "wavm",
#endif
    ""
  };
  engines.pop_back();
  return engines;
}

std::string readTestWasmFile(const std::string &filename) {
  auto path = "test/test_data/" + filename;
  std::ifstream file(path, std::ios::binary);
  EXPECT_FALSE(file.fail()) << "failed to open: " << path;
  std::stringstream file_string_stream;
  file_string_stream << file.rdbuf();
  return file_string_stream.str();
}
} // namespace proxy_wasm
