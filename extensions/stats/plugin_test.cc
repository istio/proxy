#include "absl/hash/hash_testing.h"
#include "extensions/stats/plugin.h"
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
  IstioDimensions d3{.request_protocol = "grpc", .response_code="200"};
  IstioDimensions d4{.request_protocol = "grpc", .response_code="400"};
  IstioDimensions d5{.request_protocol = "grpc", .source_app = "app_source"};
  IstioDimensions d6{.request_protocol = "grpc", .source_app = "app_source", .source_version = "v2"};
  IstioDimensions d7{.outbound = true, .request_protocol = "grpc", .source_app = "app_source", .source_version = "v2"};
  IstioDimensions d8{.outbound = true, .request_protocol = "grpc", .source_app = "app_source", .source_version = "v2"};

EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      d1,d2,d3,d4,d5,d6,d7,d8
  }));
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