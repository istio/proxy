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

#include "include/proxy-wasm/null_plugin.h"

namespace proxy_wasm {
namespace null_plugin {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Stats {

TEST(IstioDimensions, Hash) {
  IstioDimensions d1(count_standard_labels);
  IstioDimensions d2(count_standard_labels);
  d2[request_protocol] = "grpc";
  IstioDimensions d3(count_standard_labels);
  d3[request_protocol] = "grpc";
  d3[response_code] = "200";
  IstioDimensions d4(count_standard_labels);
  d4[request_protocol] = "grpc";
  d4[response_code] = "400";
  IstioDimensions d5(count_standard_labels);
  d5[request_protocol] = "grpc";
  d5[source_app] = "app_source";
  IstioDimensions d6(count_standard_labels);
  d6[reporter] = source;
  d6[request_protocol] = "grpc";
  d6[source_app] = "app_source";
  d6[source_version] = "v2";
  IstioDimensions d7(count_standard_labels);
  d7[request_protocol] = "grpc", d7[source_app] = "app_source",
  d7[source_version] = "v2";
  IstioDimensions d7_duplicate(count_standard_labels);
  d7_duplicate[request_protocol] = "grpc";
  d7_duplicate[source_app] = "app_source";
  d7_duplicate[source_version] = "v2";
  IstioDimensions d8(count_standard_labels);
  d8[request_protocol] = "grpc";
  d8[source_app] = "app_source";
  d8[source_version] = "v2";
  d8[grpc_response_status] = "12";

  // Must be unique except for d7 and d8.
  std::set<size_t> hashes;
  hashes.insert(HashIstioDimensions()(d1));
  hashes.insert(HashIstioDimensions()(d2));
  hashes.insert(HashIstioDimensions()(d3));
  hashes.insert(HashIstioDimensions()(d4));
  hashes.insert(HashIstioDimensions()(d5));
  hashes.insert(HashIstioDimensions()(d6));
  hashes.insert(HashIstioDimensions()(d7));
  hashes.insert(HashIstioDimensions()(d7_duplicate));
  hashes.insert(HashIstioDimensions()(d8));
  EXPECT_EQ(hashes.size(), 8);
}

}  // namespace Stats

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace null_plugin
}  // namespace proxy_wasm
#endif
