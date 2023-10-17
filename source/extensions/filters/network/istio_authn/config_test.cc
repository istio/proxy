/* Copyright Istio Authors. All Rights Reserved.
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

#include "source/extensions/filters/network/istio_authn/config.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/buffer/buffer_impl.h"
#include "source/common/stream_info/filter_state_impl.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"

using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace IstioAuthn {
namespace {

TEST(Principal, Basic) {
  const std::string value1 = "spiffe://cluster.local/ns/my-namespace/sa/my-account1";
  auto p1 = Principal(value1);
  EXPECT_EQ(p1.serializeAsString(), absl::make_optional<std::string>(value1));
  EXPECT_EQ(p1.principal(), value1);

  const std::string value2 = "spiffe://cluster.local/ns/my-namespace/sa/my-account2";
  auto p2 = Principal(value2);
  EXPECT_NE(p1.hash(), p2.hash());
}

TEST(Principal, GetPrincipals) {
  const std::string peer = "spiffe://cluster.local/ns/my-namespace/sa/my-account1";
  const std::string local = "spiffe://cluster.local/ns/my-namespace/sa/my-account2";
  StreamInfo::FilterStateImpl filter_state(StreamInfo::FilterState::LifeSpan::Connection);
  {
    const auto info = getPrincipals(filter_state);
    EXPECT_EQ(info.peer, "");
    EXPECT_EQ(info.local, "");
  }
  filter_state.setData(PeerPrincipalKey, std::make_shared<Principal>(peer),
                       StreamInfo::FilterState::StateType::ReadOnly,
                       StreamInfo::FilterState::LifeSpan::Connection);
  {
    const auto info = getPrincipals(filter_state);
    EXPECT_EQ(info.peer, peer);
    EXPECT_EQ(info.local, "");
  }
  filter_state.setData(LocalPrincipalKey, std::make_shared<Principal>(local),
                       StreamInfo::FilterState::StateType::ReadOnly,
                       StreamInfo::FilterState::LifeSpan::Connection);
  {
    const auto info = getPrincipals(filter_state);
    EXPECT_EQ(info.peer, peer);
    EXPECT_EQ(info.local, local);
  }
}

class IstioAuthnFilterTest : public testing::TestWithParam<bool> {
public:
  IstioAuthnFilterTest() : filter_(GetParam()) {}

protected:
  IstioAuthnFilter filter_;
};

TEST_P(IstioAuthnFilterTest, CallbacksNoSsl) {
  // Validate stubs.
  EXPECT_EQ(filter_.onNewConnection(), Network::FilterStatus::Continue);
  Buffer::OwnedImpl buffer("hello");
  EXPECT_EQ(filter_.onData(buffer, true), Network::FilterStatus::Continue);

  testing::NiceMock<Network::MockReadFilterCallbacks> callbacks;
  filter_.initializeReadFilterCallbacks(callbacks);
  EXPECT_CALL(callbacks.connection_, ssl()).WillOnce(Return(nullptr));
  callbacks.connection_.raiseEvent(Network::ConnectionEvent::Connected);
}

TEST_P(IstioAuthnFilterTest, CallbacksWithSsl) {
  testing::NiceMock<Network::MockReadFilterCallbacks> callbacks;
  filter_.initializeReadFilterCallbacks(callbacks);

  auto ssl = std::make_shared<Ssl::MockConnectionInfo>();
  const std::string peer = "spiffe://cluster.local/ns/my-namespace/sa/my-account1";
  const std::string local = "spiffe://cluster.local/ns/my-namespace/sa/my-account2";
  std::vector<std::string> peer_sans{peer};
  std::vector<std::string> local_sans{local};
  EXPECT_CALL(callbacks.connection_, ssl()).WillRepeatedly(Return(ssl));
  EXPECT_CALL(*ssl, peerCertificatePresented()).WillRepeatedly(Return(true));
  EXPECT_CALL(*ssl, uriSanPeerCertificate()).WillRepeatedly(Return(peer_sans));
  EXPECT_CALL(*ssl, uriSanLocalCertificate()).WillRepeatedly(Return(local_sans));
  callbacks.connection_.raiseEvent(Network::ConnectionEvent::Connected);
  const auto info = getPrincipals(*callbacks.connection_.stream_info_.filter_state_);
  EXPECT_EQ(info.peer, peer);
  EXPECT_EQ(info.local, local);
}

TEST_P(IstioAuthnFilterTest, CallbacksWithSslMultipleSAN) {
  testing::NiceMock<Network::MockReadFilterCallbacks> callbacks;
  filter_.initializeReadFilterCallbacks(callbacks);

  auto ssl = std::make_shared<Ssl::MockConnectionInfo>();
  const std::string spiffe1 = "spiffe://cluster.local/ns/my-namespace/sa/my-account1";
  const std::string spiffe2 = "spiffe://cluster.local/ns/my-namespace/sa/my-account2";
  std::vector<std::string> peer_sans{"test1.com", spiffe1, spiffe2};
  std::vector<std::string> local_sans{"test2.com", "test3.com"};
  EXPECT_CALL(callbacks.connection_, ssl()).WillRepeatedly(Return(ssl));
  EXPECT_CALL(*ssl, peerCertificatePresented()).WillRepeatedly(Return(true));
  EXPECT_CALL(*ssl, uriSanPeerCertificate()).WillRepeatedly(Return(peer_sans));
  EXPECT_CALL(*ssl, uriSanLocalCertificate()).WillRepeatedly(Return(local_sans));
  callbacks.connection_.raiseEvent(Network::ConnectionEvent::Connected);
  const auto info = getPrincipals(*callbacks.connection_.stream_info_.filter_state_);
  EXPECT_EQ(info.peer, spiffe1);
  EXPECT_EQ(info.local, "");
}

INSTANTIATE_TEST_SUITE_P(IstioAuthnFilterTestShared, IstioAuthnFilterTest,
                         testing::Values(true, false));

} // namespace
} // namespace IstioAuthn
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
