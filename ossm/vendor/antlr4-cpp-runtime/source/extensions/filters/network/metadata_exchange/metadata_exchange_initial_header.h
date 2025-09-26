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

#pragma once

#include <cstdint>

#include "envoy/common/platform.h"

namespace Envoy {
namespace Tcp {
namespace MetadataExchange {

// Used with MetadataExchangeHeaderProto to be extensible.
PACKED_STRUCT(struct MetadataExchangeInitialHeader {
  uint32_t magic; // Magic number in network byte order. Most significant byte
                  // is placed first.
  static const uint32_t magic_number = 0x3D230467; // decimal 1025705063
  uint32_t data_size; // Size of the data blob in network byte order. Most
                      // significant byte is placed first.
});

} // namespace MetadataExchange
} // namespace Tcp
} // namespace Envoy