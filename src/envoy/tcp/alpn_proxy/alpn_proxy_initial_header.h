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

namespace Envoy {
namespace Tcp {
namespace AlpnProxy {

// Used with AlpnProxyHeaderProto to be extensible.
struct AlpnProxyInitialHeader {
  uint32_t magic;  // Magic number in network byte order. Most significant byte
                   // is placed first.
  static const uint32_t magic_number = 0x23071961;  // decimal 587667809
  uint32_t data_size;  // Size of the data blob in network byte order. Most
                       // significant byte is placed first.
} __attribute__((packed));

}  // namespace AlpnProxy
}  // namespace Tcp
}  // namespace Envoy