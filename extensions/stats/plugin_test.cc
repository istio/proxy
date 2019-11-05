/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "extensions/stats/plugin.h"

#include <set>

#include "absl/hash/hash_testing.h"
#include "gtest/gtest.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "extensions/common/wasm/null/null.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

TEST(IstioDimensions, Hash) {
  IstioDimensions d1;
  IstioDimensions d2{.request_protocol = "grpc"};
  IstioDimensions d3{.request_protocol = "grpc", .response_code = "200"};
  IstioDimensions d4{.request_protocol = "grpc", .response_code = "400"};
  IstioDimensions d5{.request_protocol = "grpc", .source_app = "app_source"};
  IstioDimensions d6{.request_protocol = "grpc",
                     .source_app = "app_source",
                     .source_version = "v2"};
  IstioDimensions d7{.outbound = true,
                     .request_protocol = "grpc",
                     .source_app = "app_source",
                     .source_version = "v2"};
  IstioDimensions d8{.outbound = true,
                     .request_protocol = "grpc",
                     .source_app = "app_source",
                     .source_version = "v2"};
  // Must be unique except for d7 and d8.
  std::set<size_t> hashes;
  hashes.insert(IstioDimensions::HashIstioDimensions()(d1));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d2));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d3));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d4));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d5));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d6));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d7));
  hashes.insert(IstioDimensions::HashIstioDimensions()(d8));
  EXPECT_EQ(hashes.size(), 7);
}

}  // namespace Stats

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
