/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include <string>

#include "src/envoy/tcp/sni_verifier/config.h"
#include "src/envoy/tcp/sni_verifier/sni_verifier.h"

#include "common/buffer/buffer_impl.h"

#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/test_common/tls_utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

// Test that a SniVerifier filter config works.
TEST(SniVerifierTest, ConfigTest) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SniVerifierConfigFactory factory;

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(
      *factory.createEmptyConfigProto(), context);
  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

class SniVerifierFilterTest : public testing::Test {
 protected:
  void SetUp() override {
    store_ = std::make_unique<Stats::IsolatedStoreImpl>();
    cfg_ = std::make_shared<Config>(*store_);
    filter_ = std::make_unique<SniVerifierFilter>(cfg_);
  }

  void TearDown() override {
    filter_ = nullptr;
    cfg_ = nullptr;
    store_ = nullptr;
  }

  void runTest(std::string outer_sni, std::string inner_sni,
               Network::FilterStatus expected_status) {
    NiceMock<Network::MockReadFilterCallbacks> filter_callbacks;

    ON_CALL(filter_callbacks.connection_, requestedServerName())
        .WillByDefault(Return(outer_sni));

    filter_->initializeReadFilterCallbacks(filter_callbacks);
    filter_->onNewConnection();

    auto client_hello = Tls::Test::generateClientHello(inner_sni, "");
    Buffer::OwnedImpl data;
    data.add(client_hello.data(), client_hello.size());

    EXPECT_EQ(expected_status, filter_->onData(data, true));
  }

  ConfigSharedPtr cfg_;

 private:
  std::unique_ptr<SniVerifierFilter> filter_;
  std::unique_ptr<Stats::Scope> store_;
};

TEST_F(SniVerifierFilterTest, SnisMatch) {
  runTest("www.example.com", "www.example.com",
          Network::FilterStatus::Continue);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SnisDoNotMatch) {
  runTest("www.example.com", "istio.io", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, EmptyOuterSni) {
  runTest("", "istio.io", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, EmptyInnerSni) {
  runTest("www.example.com", "", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, BothSnisEmpty) {
  runTest("", "", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

}  // namespace SniVerifier
}  // namespace Tcp
}  // namespace Envoy
