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

#include "extensions/attributegen/plugin.h"

#include <set>

#include "gtest/gtest.h"

// WASM_PROLOG
#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"

#else  // NULL_PLUGIN

#include "common/buffer/buffer_impl.h"
#include "common/http/message_impl.h"
#include "common/stats/isolated_store_impl.h"
#include "common/stream_info/stream_info_impl.h"
#include "envoy/server/lifecycle_notifier.h"
#include "extensions/common/wasm/null/null.h"
#include "extensions/common/wasm/wasm.h"
#include "extensions/common/wasm/wasm_state.h"
#include "extensions/filters/http/wasm/wasm_filter.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/grpc/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/mocks.h"
#include "test/test_common/environment.h"
#include "test/test_common/printers.h"
#include "test/test_common/utility.h"

using testing::_;
using testing::AtLeast;
using testing::Eq;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Wasm {

using envoy::config::core::v3::TrafficDirection;
using Envoy::Extensions::Common::Wasm::PluginSharedPtr;
using Envoy::Extensions::Common::Wasm::Wasm;
using Envoy::Extensions::Common::Wasm::WasmHandleSharedPtr;
using Envoy::Extensions::Common::Wasm::WasmState;
using GrpcService = envoy::config::core::v3::GrpcService;
using WasmFilterConfig = envoy::extensions::filters::http::wasm::v3::Wasm;

class TestFilter : public Envoy::Extensions::Common::Wasm::Context {
 public:
  TestFilter(Wasm* wasm, uint32_t root_context_id,
             Envoy::Extensions::Common::Wasm::PluginSharedPtr plugin,
             bool mock_logger = false)
      : Envoy::Extensions::Common::Wasm::Context(wasm, root_context_id, plugin),
        mock_logger_(mock_logger) {}

  void scriptLog(spdlog::level::level_enum level,
                 absl::string_view message) override {
    if (mock_logger_) {
      scriptLog_(level, message);
    }
  }
  MOCK_METHOD2(scriptLog_, void(spdlog::level::level_enum level,
                                absl::string_view message));

 private:
  bool mock_logger_;
};

class TestRoot : public Envoy::Extensions::Common::Wasm::Context {
 public:
  TestRoot(bool mock_logger = false) : mock_logger_(mock_logger) {}

  void scriptLog(spdlog::level::level_enum level,
                 absl::string_view message) override {
    if (mock_logger_) {
      scriptLog_(level, message);
    }
  }
  MOCK_METHOD2(scriptLog_, void(spdlog::level::level_enum level,
                                absl::string_view message));

 private:
  bool mock_logger_;
};

struct TestParams {
  std::string runtime;  // null, v8, wavm
  // In order to load wasm files we need to specify base path relative to
  // WORKSPACE.
  std::string wasmfiles_dir;
};

std::ostream& operator<<(std::ostream& os, const TestParams& s) {
  return (os << "{runtime: '" << s.runtime << "', wasmfiles_dir: '"
             << s.wasmfiles_dir << "' }");
}

class WasmHttpFilterTest : public testing::TestWithParam<TestParams> {
 public:
  WasmHttpFilterTest() {}
  ~WasmHttpFilterTest() {}

  void setupConfig(const std::string& name, std::string plugin_config,
                   bool add_filter = true, std::string root_id = "",
                   bool mock_logger = false) {
    auto params = GetParam();
    auto code =
        (params.runtime == "null")
            ? name
            : TestEnvironment::readFileToStringForTest(
                  TestEnvironment::substitute(
                      "{{ test_rundir }}" + params.wasmfiles_dir + "/" + name));

    root_context_ = new TestRoot(mock_logger);
    WasmFilterConfig proto_config;
    proto_config.mutable_config()->mutable_vm_config()->set_vm_id("vm_id");
    proto_config.mutable_config()->mutable_vm_config()->set_runtime(
        "envoy.wasm.runtime.null");
    proto_config.mutable_config()
        ->mutable_vm_config()
        ->mutable_code()
        ->mutable_local()
        ->set_inline_bytes(code);
    proto_config.mutable_config()->set_configuration(plugin_config);
    Api::ApiPtr api = Api::createApiForTest(stats_store_);
    scope_ = Stats::ScopeSharedPtr(stats_store_.createScope("wasm."));
    auto vm_id = "";
    plugin_ = std::make_shared<Extensions::Common::Wasm::Plugin>(
        name, root_id, vm_id, TrafficDirection::INBOUND, local_info_,
        &listener_metadata_);
    // creates a base VM
    // This is synchronous, even though it happens thru a callback due to null
    // vm.
    Extensions::Common::Wasm::createWasmForTesting(
        proto_config.config().vm_config(), plugin_, scope_, cluster_manager_,
        init_manager_, dispatcher_, random_, *api,
        std::unique_ptr<Envoy::Extensions::Common::Wasm::Context>(
            root_context_),
        remote_data_provider_,
        [this](WasmHandleSharedPtr wasm) { wasm_ = wasm; });
    // wasm_ is set correctly
    // This will only call onStart.
    auto config_status =
        wasm_->wasm()->configure(root_context_, plugin_, plugin_config);
    if (!config_status) {
      throw EnvoyException("Configuration failed");
    }
    if (add_filter) {
      setupFilter(root_id, mock_logger);
    }
  }

  void setupFilter(const std::string root_id = "", bool mock_logger = false) {
    filter_ = std::make_unique<TestFilter>(
        wasm_->wasm().get(), wasm_->wasm()->getRootContext(root_id)->id(),
        plugin_, mock_logger);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);

    ON_CALL(decoder_callbacks_.stream_info_, filterState())
        .WillByDefault(ReturnRef(request_stream_info_.filterState()));
    ON_CALL(encoder_callbacks_.stream_info_, filterState())
        .WillByDefault(ReturnRef(request_stream_info_.filterState()));
  }

  void makeTestRequest(Http::TestRequestHeaderMapImpl& request_headers,
                       Http::TestResponseHeaderMapImpl& response_headers,
                       std::string bdata = "data") {
    auto fs = request_stream_info_.filterState();

    EXPECT_EQ(Http::FilterHeadersStatus::Continue,
              filter_->decodeHeaders(request_headers, true));

    Buffer::OwnedImpl data(bdata);
    EXPECT_EQ(Http::FilterDataStatus::Continue,
              filter_->decodeData(data, true));

    EXPECT_EQ(Http::FilterHeadersStatus::Continue,
              filter_->encodeHeaders(response_headers, true));

    filter_->log(&request_headers, nullptr, nullptr, request_stream_info_);
  }

  // Many of the following are not used yet, but are useful
  Stats::IsolatedStoreImpl stats_store_;
  Stats::ScopeSharedPtr scope_;
  NiceMock<ThreadLocal::MockInstance> tls_;
  NiceMock<Event::MockDispatcher> dispatcher_;
  NiceMock<Runtime::MockRandomGenerator> random_;
  NiceMock<Upstream::MockClusterManager> cluster_manager_;
  NiceMock<Init::MockManager> init_manager_;
  WasmHandleSharedPtr wasm_;
  PluginSharedPtr plugin_;
  std::unique_ptr<TestFilter> filter_;
  NiceMock<Envoy::Ssl::MockConnectionInfo> ssl_;
  NiceMock<Envoy::Network::MockConnection> connection_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> request_stream_info_;
  NiceMock<LocalInfo::MockLocalInfo> local_info_;
  envoy::config::core::v3::Metadata listener_metadata_;
  TestRoot* root_context_ = nullptr;
  Config::DataSource::RemoteAsyncDataProviderPtr remote_data_provider_;
};

}  // namespace Wasm
}  // namespace HttpFilters
namespace Common {
namespace Wasm {
namespace Null {
namespace Plugin {

#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace AttributeGen {

using HttpFilters::Wasm::TestParams;
using HttpFilters::Wasm::WasmHttpFilterTest;

std::vector<TestParams> generateTestParams() {
  return std::vector<TestParams>{
      {.runtime = "null"},
      // {.runtime = "v8", .wasmfiles_dir =
      // "/extensions/attributegen/testdata"},
  };
}

INSTANTIATE_TEST_SUITE_P(Runtimes, WasmHttpFilterTest,
                         testing::ValuesIn(generateTestParams()));

// Bad code in initial config.
TEST_P(WasmHttpFilterTest, BadCode) {
  if (GetParam().runtime == "null") {
    return;
  }
  EXPECT_THROW_WITH_MESSAGE(setupConfig("bad code", "badonfig"),
                            Common::Wasm::WasmException,
                            "Failed to initialize WASM code from <inline>");
}

TEST_P(WasmHttpFilterTest, OneMatch) {
  const char* plugin_config = R"EOF(
                    {"attributes": [{"output_attribute": "istio.operationId",
                    "match": [{"value":
                            "GetStatus", "condition": "request.url_path.startsWith('/status')"}]}]}
  )EOF";
  setupConfig("envoy.wasm.attributegen", plugin_config);

  Http::TestRequestHeaderMapImpl request_headers{{":path", "/status/207"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "404"}};

  WasmHttpFilterTest::makeTestRequest(request_headers, response_headers);

  auto fs = request_stream_info_.filterState();
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId"), true);
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId_error"), false);
  const auto& operationId = fs->getDataReadOnly<WasmState>("istio.operationId");
  ASSERT_EQ(operationId.value(), "GetStatus");
}

TEST_P(WasmHttpFilterTest, ExprEvalError) {
  const char* plugin_config = R"EOF(
                    {"attributes": [{"output_attribute": "istio.operationId",
                    "match": [{"value":
                            "GetStatus", "condition": "request.url_path"}]}]}
  )EOF";
  setupConfig("envoy.wasm.attributegen", plugin_config);

  Http::TestRequestHeaderMapImpl request_headers{{":path", "/status/207"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "404"}};

  WasmHttpFilterTest::makeTestRequest(request_headers, response_headers);

  auto fs = request_stream_info_.filterState();
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId"), false);
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId_error"), true);
}

TEST_P(WasmHttpFilterTest, UnparseableConfig) {
  const char* plugin_config = R"EOF(
                    attributes = [ output_attribute ]; 
  )EOF";
  EXPECT_THROW_WITH_MESSAGE(
      setupConfig("envoy.wasm.attributegen", plugin_config), EnvoyException,
      "Configuration failed");
}

TEST_P(WasmHttpFilterTest, BadExpr) {
  const char* plugin_config = R"EOF(
                    {"attributes": [{"output_attribute": "istio.operationId",
                    "match": [{"value":
                            "GetStatus", "condition": "if a = b then return 5"}]}]}
  )EOF";
  EXPECT_THROW_WITH_MESSAGE(
      setupConfig("envoy.wasm.attributegen", plugin_config), EnvoyException,
      "Configuration failed");
}

TEST_P(WasmHttpFilterTest, NoMatch) {
  const char* plugin_config = R"EOF(
                    {"attributes": [{"output_attribute": "istio.operationId",
                    "match": [{"value":
                            "GetStatus", "condition": "request.url_path.startsWith('/status') && request.method == 'POST'"}]}]}
  )EOF";
  setupConfig("envoy.wasm.attributegen", plugin_config);

  Http::TestRequestHeaderMapImpl request_headers{{":path", "/status/207"},
                                                 {":method", "GET"}};
  Http::TestResponseHeaderMapImpl response_headers{{":status", "404"}};

  WasmHttpFilterTest::makeTestRequest(request_headers, response_headers);

  auto fs = request_stream_info_.filterState();
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId"), false);
  ASSERT_EQ(fs->hasData<WasmState>("istio.operationId_error"), false);
}
}  // namespace AttributeGen

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Plugin
}  // namespace Null
}  // namespace Wasm
}  // namespace Common
}  // namespace Extensions
}  // namespace Envoy
#endif
