
/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "src/envoy/alts/alts_socket_factory.h"
#include <common/protobuf/protobuf.h>
#include <envoy/server/transport_socket_config.h>
#include "common/common/assert.h"
#include "envoy/registry/registry.h"
#include "grpc/grpc_security.h"
#include "src/core/tsi/alts/handshaker/alts_tsi_handshaker.h"
#include "src/envoy/alts/tsi_handshaker.h"
#include "tsi_transport_socket.h"

namespace Envoy {
namespace Server {
namespace Configuration {

ProtobufTypes::MessagePtr
AltsTransportSocketConfigFactory::createEmptyConfigProto() {
  return std::make_unique<ProtobufWkt::Empty>();
}

std::string
Envoy::Server::Configuration::AltsTransportSocketConfigFactory::name() const {
  return "alts";
}

Network::TransportSocketFactoryPtr
UpstreamAltsTransportSocketConfigFactory::createTransportSocketFactory(
    const Protobuf::Message &, TransportSocketFactoryContext &) {
  return std::make_unique<Security::TsiSocketFactory>([]() {
    grpc_alts_credentials_options *options =
        grpc_alts_credentials_client_options_create();

    tsi_handshaker *handshaker = nullptr;

    alts_tsi_handshaker_create(options, "target_name", "localhost:8080",
                               true /* is_client */, &handshaker);

    ASSERT(handshaker != nullptr);

    grpc_alts_credentials_options_destroy(options);

    return std::make_unique<Security::TsiHandshaker>(handshaker);
  });
}

Network::TransportSocketFactoryPtr
DownstreamAltsTransportSocketConfigFactory::createTransportSocketFactory(
    const std::string &, const std::vector<std::string> &, bool,
    const Protobuf::Message &, TransportSocketFactoryContext &) {
  return std::make_unique<Security::TsiSocketFactory>([]() {
    grpc_alts_credentials_options *options =
        grpc_alts_credentials_server_options_create();

    tsi_handshaker *handshaker = nullptr;

    alts_tsi_handshaker_create(options, nullptr, "localhost:8080",
                               false /* is_client */, &handshaker);

    ASSERT(handshaker != nullptr);

    grpc_alts_credentials_options_destroy(options);

    return std::make_unique<Security::TsiHandshaker>(handshaker);
  });
}

static Registry::RegisterFactory<UpstreamAltsTransportSocketConfigFactory,
                                 UpstreamTransportSocketConfigFactory>
    upstream_registered_;

static Registry::RegisterFactory<DownstreamAltsTransportSocketConfigFactory,
                                 DownstreamTransportSocketConfigFactory>
    downstream_registered_;
}
}
}
