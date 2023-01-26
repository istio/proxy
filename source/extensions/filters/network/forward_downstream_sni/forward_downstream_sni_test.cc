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

#include "source/extensions/filters/network/forward_downstream_sni/forward_downstream_sni.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "source/common/network/upstream_server_name.h"
#include "source/extensions/filters/network/forward_downstream_sni/config.h"
#include "test/mocks/network/mocks.h"
#include "test/mocks/server/mocks.h"
#include "test/mocks/stream_info/mocks.h"

using testing::_;
using testing::Matcher;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace Envoy {
namespace Tcp {
namespace ForwardDownstreamSni {

using ::Envoy::Network::UpstreamServerName;

// Test that a ForwardDownstreamSni filter config works.
TEST(ForwardDownstreamSni, ConfigTest) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  ForwardDownstreamSniNetworkFilterConfigFactory factory;

  Network::FilterFactoryCb cb =
      factory.createFilterFactoryFromProto(*factory.createEmptyConfigProto(), context);
  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

// Test that forward requested server name is set if SNI is available
TEST(ForwardDownstreamSni, SetUpstreamServerNameOnlyIfSniIsPresent) {
  NiceMock<Network::MockReadFilterCallbacks> filter_callbacks;

  NiceMock<StreamInfo::MockStreamInfo> stream_info;
  ON_CALL(filter_callbacks.connection_, streamInfo()).WillByDefault(ReturnRef(stream_info));
  ON_CALL(Const(filter_callbacks.connection_), streamInfo()).WillByDefault(ReturnRef(stream_info));

  ForwardDownstreamSniFilter filter;
  filter.initializeReadFilterCallbacks(filter_callbacks);

  // no sni
  {
    ON_CALL(filter_callbacks.connection_, requestedServerName())
        .WillByDefault(Return(EMPTY_STRING));
    filter.onNewConnection();

    EXPECT_FALSE(stream_info.filterState()->hasData<UpstreamServerName>(UpstreamServerName::key()));
  }

  // with sni
  {
    ON_CALL(filter_callbacks.connection_, requestedServerName())
        .WillByDefault(Return("www.example.com"));
    filter.onNewConnection();

    EXPECT_TRUE(stream_info.filterState()->hasData<UpstreamServerName>(UpstreamServerName::key()));

    auto forward_requested_server_name =
        stream_info.filterState()->getDataReadOnly<UpstreamServerName>(UpstreamServerName::key());
    EXPECT_EQ(forward_requested_server_name->value(), "www.example.com");
  }
}

} // namespace ForwardDownstreamSni
} // namespace Tcp
} // namespace Envoy
