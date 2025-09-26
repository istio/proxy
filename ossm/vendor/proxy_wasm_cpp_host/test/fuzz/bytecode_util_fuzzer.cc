// Copyright 2022 Google LLC
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

#include "include/proxy-wasm/bytecode_util.h"

#include <string>

namespace proxy_wasm {
namespace {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  auto bytecode = std::string_view(reinterpret_cast<const char *>(data), size);

  AbiVersion version;
  BytecodeUtil::getAbiVersion(bytecode, version);

  std::string_view custom_section;
  BytecodeUtil::getCustomSection(bytecode, "precompiled", custom_section);

  std::string stripped_source;
  BytecodeUtil::getStrippedSource(bytecode, stripped_source);

  std::unordered_map<uint32_t, std::string> function_names;
  BytecodeUtil::getFunctionNameIndex(bytecode, function_names);

  return 0;
}

} // namespace
} // namespace proxy_wasm
