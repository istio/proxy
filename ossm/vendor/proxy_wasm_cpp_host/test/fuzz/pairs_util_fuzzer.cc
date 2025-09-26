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

#include "include/proxy-wasm/pairs_util.h"

#include <cstdint>
#include <cstring>

namespace proxy_wasm {
namespace {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  auto input = std::string_view(reinterpret_cast<const char *>(data), size);

  auto pairs = PairsUtil::toPairs(input);

  if (!pairs.empty()) {
    // Verify that non-empty Pairs serializes back to the same bytes.
    auto new_size = PairsUtil::pairsSize(pairs);
    if (new_size != size) {
      __builtin_trap();
    }
    std::vector<char> new_data(new_size);
    if (!PairsUtil::marshalPairs(pairs, new_data.data(), new_data.size())) {
      __builtin_trap();
    }
    if (::memcmp(new_data.data(), data, size) != 0) {
      __builtin_trap();
    }
  }

  return 0;
}

} // namespace
} // namespace proxy_wasm
