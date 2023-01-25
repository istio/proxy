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

#include "source/extensions/filters/network/tcp_cluster_rewrite/config.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/mocks/server/mocks.h"

using namespace ::istio::envoy::config::filter::network::tcp_cluster_rewrite;
using testing::_;

namespace Envoy {
namespace Tcp {
namespace TcpClusterRewrite {

TEST(ConfigTest, ConfigTest) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  TcpClusterRewriteFilterConfigFactory factory;
  v2alpha1::TcpClusterRewrite config =
      *dynamic_cast<v2alpha1::TcpClusterRewrite*>(factory.createEmptyConfigProto().get());

  config.set_cluster_pattern("connection\\.sni");
  config.set_cluster_replacement("replacement.sni");

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(config, context);
  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

} // namespace TcpClusterRewrite
} // namespace Tcp
} // namespace Envoy
