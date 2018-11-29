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

#include "src/envoy/tcp/sni_verifier/config.h"
#include "src/envoy/tcp/sni_verifier/sni_verifier.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

// Test that a SniVerifier filter config works.
TEST(SniVerifier, ConfigTest) {
  NiceMock<Server::Configuration::MockFactoryContext> context;
  SniVerifierConfigFactory factory;

  Network::FilterFactoryCb cb = factory.createFilterFactoryFromProto(
      *factory.createEmptyConfigProto(), context);
  Network::MockConnection connection;
  EXPECT_CALL(connection, addReadFilter(_));
  cb(connection);
}

}  // namespace SniVerifier
}  // namespace Tcp
}  // namespace Envoy
