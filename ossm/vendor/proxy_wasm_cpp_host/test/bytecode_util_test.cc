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

#include "include/proxy-wasm/bytecode_util.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "test/utility.h"

#include "gtest/gtest.h"

namespace proxy_wasm {

TEST(TestBytecodeUtil, getCustomSection) {
  std::string custom_section = {
      0x00, 0x61, 0x73, 0x6d,       // Wasm magic
      0x01, 0x00, 0x00, 0x00,       // Wasm version
      0x00,                         // custom section id
      0x0a,                         // section length
      0x04, 0x68, 0x65, 0x79, 0x21, // section name: "hey!"
      0x68, 0x65, 0x6c, 0x6c, 0x6f, // content: "hello"
  };
  std::string_view section = {};

  // OK.
  EXPECT_TRUE(BytecodeUtil::getCustomSection(custom_section, "hey!", section));
  EXPECT_EQ(std::string(section), "hello");
  section = {};

  // Non exist.
  EXPECT_TRUE(BytecodeUtil::getCustomSection(custom_section, "non-exist", section));
  EXPECT_EQ(section, "");

  // Fail due to the corrupted bytecode.
  // TODO(@mathetake): here we haven't covered all the parsing failure branches. Add more cases.
  std::string corrupted = {custom_section.data(),
                           custom_section.data() + custom_section.size() - 3};
  EXPECT_FALSE(BytecodeUtil::getCustomSection(corrupted, "hey", section));
  corrupted = {custom_section.data() + 1, custom_section.data() + custom_section.size()};
  EXPECT_FALSE(BytecodeUtil::getCustomSection(corrupted, "hey", section));
}

TEST(TestBytecodeUtil, getFunctionNameIndex) {
  const auto source = readTestWasmFile("abi_export.wasm");
  std::unordered_map<uint32_t, std::string> actual;
  // OK.
  EXPECT_TRUE(BytecodeUtil::getFunctionNameIndex(source, actual));
  EXPECT_FALSE(actual.empty());
  bool abi_version_found = false;
  for (const auto &it : actual) {
    if (it.second == "proxy_abi_version_0_2_0") {
      abi_version_found = true;
      break;
    }
  }
  EXPECT_TRUE(abi_version_found);

  // Fail due to the corrupted bytecode.
  // TODO(@mathetake): here we haven't covered all the parsing failure branches. Add more cases.
  actual = {};
  std::string_view name_section = {};
  EXPECT_TRUE(BytecodeUtil::getCustomSection(source, "name", name_section));
  // Passing module with malformed custom section.
  std::string corrupted = {source.data(), name_section.data() + 1};
  EXPECT_FALSE(BytecodeUtil::getFunctionNameIndex(corrupted, actual));
  EXPECT_TRUE(actual.empty());
}

TEST(TestBytecodeUtil, getStrippedSource) {
  // Unmodified case.
  auto source = readTestWasmFile("abi_export.wasm");
  std::string actual;
  EXPECT_TRUE(BytecodeUtil::getStrippedSource(source, actual));
  // If no `precompiled_` is found in the custom sections,
  // then the copy of the original should be returned.
  EXPECT_FALSE(actual.empty());
  EXPECT_TRUE(actual.data() != source.data());
  EXPECT_EQ(actual, source);

  // Append "precompiled_test" custom section
  std::vector<char> custom_section = {// custom section id
                                      0x00,
                                      // section length
                                      0x13,
                                      // name length
                                      0x10,
                                      // name = precompiled_test
                                      0x70, 0x72, 0x65, 0x63, 0x6f, 0x6d, 0x70, 0x69, 0x6c, 0x65,
                                      0x64, 0x5f, 0x74, 0x65, 0x73, 0x74,
                                      // content
                                      0x01, 0x01};

  source.append(custom_section.data(), custom_section.size());
  std::string_view section = {};
  EXPECT_TRUE(BytecodeUtil::getCustomSection(source, "precompiled_test", section));
  EXPECT_FALSE(section.empty());

  // Chcek if the custom section is stripped.
  actual = {};
  EXPECT_TRUE(BytecodeUtil::getStrippedSource(source, actual));
  // No `precompiled_` is found in the custom sections.
  EXPECT_FALSE(actual.empty());
  EXPECT_EQ(actual.size(), source.size() - custom_section.size());
}

TEST(TestBytecodeUtil, getAbiVersion) {
  const auto source = readTestWasmFile("abi_export.wasm");
  proxy_wasm::AbiVersion actual;
  EXPECT_TRUE(BytecodeUtil::getAbiVersion(source, actual));
  EXPECT_EQ(actual, proxy_wasm::AbiVersion::ProxyWasm_0_2_0);
}

} // namespace proxy_wasm
