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
#include "gtest/gtest.h"

namespace proxy_wasm {

TEST(PairsUtilTest, EncodeDecode) {
  proxy_wasm::Pairs pairs1;
  std::string data1("some_data");
  auto size_str = std::to_string(data1.size());
  pairs1.push_back({data1, size_str});
  std::vector<char> buffer(PairsUtil::pairsSize(pairs1));
  EXPECT_TRUE(PairsUtil::marshalPairs(pairs1, buffer.data(), buffer.size()));
  auto pairs2 = PairsUtil::toPairs(std::string_view(buffer.data(), buffer.size()));
  EXPECT_EQ(pairs2.size(), pairs1.size());
  EXPECT_EQ(pairs2[0].first, pairs1[0].first);
  EXPECT_EQ(pairs2[0].second, pairs1[0].second);
}

} // namespace proxy_wasm
