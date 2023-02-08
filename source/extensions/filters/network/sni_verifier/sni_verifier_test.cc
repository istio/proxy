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

#include "source/extensions/filters/network/sni_verifier/sni_verifier.h"

#include <climits>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/extensions/filters/network/sni_verifier/config.h"
#include "test/extensions/filters/listener/tls_inspector/tls_utility.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"

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

  Network::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(*factory.createEmptyConfigProto(), context);
  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

class SniVerifierFilterTest : public testing::Test {
protected:
  static constexpr size_t TLS_MAX_CLIENT_HELLO = 250;

  void SetUp() override {
    store_ = std::make_unique<Stats::IsolatedStoreImpl>();
    cfg_ = std::make_shared<Config>(*store_->rootScope(), TLS_MAX_CLIENT_HELLO);
    filter_ = std::make_unique<Filter>(cfg_);
  }

  void TearDown() override {
    filter_ = nullptr;
    cfg_ = nullptr;
    store_ = nullptr;
  }

  void runTestForClientHello(std::string outer_sni, std::string inner_sni,
                             Network::FilterStatus expected_status,
                             size_t data_installment_size = UINT_MAX) {
    auto client_hello = Tls::Test::generateClientHello(TLS1_VERSION, TLS1_3_VERSION, inner_sni, "");
    runTestForData(outer_sni, client_hello, expected_status, data_installment_size);
  }

  void runTestForData(std::string outer_sni, std::vector<uint8_t>& data,
                      Network::FilterStatus expected_status,
                      size_t data_installment_size = UINT_MAX) {
    NiceMock<Network::MockReadFilterCallbacks> filter_callbacks;

    ON_CALL(filter_callbacks.connection_, requestedServerName()).WillByDefault(Return(outer_sni));

    filter_->initializeReadFilterCallbacks(filter_callbacks);
    filter_->onNewConnection();

    size_t sent_data = 0;
    size_t remaining_data_to_send = data.size();
    auto status = Network::FilterStatus::StopIteration;

    while (remaining_data_to_send > 0) {
      size_t data_to_send_size = data_installment_size < remaining_data_to_send
                                     ? data_installment_size
                                     : remaining_data_to_send;
      Buffer::OwnedImpl buf;
      buf.add(data.data() + sent_data, data_to_send_size);
      status = filter_->onData(buf, true);
      sent_data += data_to_send_size;
      remaining_data_to_send -= data_to_send_size;
      if (remaining_data_to_send > 0) {
        // expect that until the whole hello message is parsed, the status is
        // stop iteration
        EXPECT_EQ(Network::FilterStatus::StopIteration, status);
      }
    }

    EXPECT_EQ(expected_status, status);
  }

  ConfigSharedPtr cfg_;

private:
  std::unique_ptr<Filter> filter_;
  std::unique_ptr<Stats::IsolatedStoreImpl> store_;
};

constexpr size_t SniVerifierFilterTest::TLS_MAX_CLIENT_HELLO; // definition

TEST_F(SniVerifierFilterTest, SnisMatch) {
  runTestForClientHello("example.com", "example.com", Network::FilterStatus::Continue);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SnisDoNotMatch) {
  runTestForClientHello("example.com", "istio.io", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, EmptyOuterSni) {
  runTestForClientHello("", "istio.io", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, EmptyInnerSni) {
  runTestForClientHello("example.com", "", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, BothSnisEmpty) {
  runTestForClientHello("", "", Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SniTooLarge) {
  runTestForClientHello("example.com", std::string(TLS_MAX_CLIENT_HELLO, 'a'),
                        Network::FilterStatus::StopIteration);
  EXPECT_EQ(1, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(0, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SnisMatchSendDataInChunksOfTen) {
  runTestForClientHello("example.com", "example.com", Network::FilterStatus::Continue, 10);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SnisMatchSendDataInChunksOfFifty) {
  runTestForClientHello("example.com", "example.com", Network::FilterStatus::Continue, 50);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, SnisMatchSendDataInChunksOfHundred) {
  runTestForClientHello("example.com", "example.com", Network::FilterStatus::Continue, 100);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(1, cfg_->stats().tls_found_.value());
  EXPECT_EQ(0, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(1, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

TEST_F(SniVerifierFilterTest, NonTLS) {
  std::vector<uint8_t> nonTLSData(TLS_MAX_CLIENT_HELLO, 7);
  runTestForData("example.com", nonTLSData, Network::FilterStatus::StopIteration);
  EXPECT_EQ(0, cfg_->stats().client_hello_too_large_.value());
  EXPECT_EQ(0, cfg_->stats().tls_found_.value());
  EXPECT_EQ(1, cfg_->stats().tls_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_found_.value());
  EXPECT_EQ(0, cfg_->stats().inner_sni_not_found_.value());
  EXPECT_EQ(0, cfg_->stats().snis_do_not_match_.value());
}

} // namespace SniVerifier
} // namespace Tcp
} // namespace Envoy
