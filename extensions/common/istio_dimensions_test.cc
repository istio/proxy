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

#include "extensions/common/istio_dimensions.h"

#include "absl/hash/hash_testing.h"
#include "gtest/gtest.h"

namespace Wasm {
namespace Common {
namespace {

TEST(WasmCommonIstioDimensionsTest, VerifyHashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      IstioDimensions{},
      IstioDimensions().set_request_protocol("wrpc"),
      IstioDimensions().set_request_protocol("grpc").set_response_code("200"),
      IstioDimensions().set_request_protocol("grpc").set_response_code("400"),
      IstioDimensions().set_source_app("app_source").set_request_protocol("grpc"),
      IstioDimensions()
          .set_source_app("app_source")
          .set_source_version("v2")
          .set_request_protocol("grpc"),
      IstioDimensions()
          .set_source_app("app_source")
          .set_source_version("v2")
          .set_request_protocol("grpc")
          .set_outbound(true),
      IstioDimensions()
          .set_source_app("app_source")
          .set_source_version("v2")
          .set_request_protocol("grpc")
          .set_outbound(true),
      IstioDimensions()
          .set_source_app("app_source")
          .set_source_version("v2")
          .set_request_protocol("grpc")
          .set_grpc_response_status("12")
          .set_outbound(true),
  }));
}

} // namespace
} // namespace Common
} // namespace Wasm
