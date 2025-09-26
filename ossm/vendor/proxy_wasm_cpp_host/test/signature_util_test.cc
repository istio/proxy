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

#include "include/proxy-wasm/signature_util.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "test/utility.h"

#include "gtest/gtest.h"

namespace proxy_wasm {

TEST(TestSignatureUtil, GoodSignature) {
#ifndef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY
  FAIL() << "Built without a key for verifying signed Wasm modules.";
#endif

  const auto bytecode = readTestWasmFile("abi_export.signed.with.key1.wasm");
  std::string message;
  EXPECT_TRUE(SignatureUtil::verifySignature(bytecode, message));
  EXPECT_EQ(message, "Wasm signature OK (Ed25519)");
}

TEST(TestSignatureUtil, BadSignature) {
#ifndef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY
  FAIL() << "Built without a key for verifying signed Wasm modules.";
#endif

  const auto bytecode = readTestWasmFile("abi_export.signed.with.key2.wasm");
  std::string message;
  EXPECT_FALSE(SignatureUtil::verifySignature(bytecode, message));
  EXPECT_EQ(message, "Signature mismatch");
}

TEST(TestSignatureUtil, NoSignature) {
#ifndef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY
  FAIL() << "Built without a key for verifying signed Wasm modules.";
#endif

  const auto bytecode = readTestWasmFile("abi_export.wasm");
  std::string message;
  EXPECT_FALSE(SignatureUtil::verifySignature(bytecode, message));
  EXPECT_EQ(message, "Custom Section \"signature_wasmsign\" not found");
}

} // namespace proxy_wasm
