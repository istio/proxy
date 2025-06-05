// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// From
// https://github.com/envoyproxy/envoy/blob/master/source/common/common/random_generator.{h,cc}

#include "source/utils/random_generator.h"

namespace cpp2sky {

std::string RandomGeneratorImpl::uuid() {
  static thread_local char buffered[2048];
  static thread_local size_t buffered_idx = sizeof(buffered);

  if (buffered_idx + 16 > sizeof(buffered)) {
    // TODO(shikugawa): Self-implemented random number generator is used right
    // now. RAND_bytes on OpenSSL is used on Envoy's RandomGenerator.
    randomBuffer(buffered, sizeof(buffered));
    buffered_idx = 0;
  }

  // Consume 16 bytes from the buffer.
  assert(buffered_idx + 16 <= sizeof(buffered));
  char* rand = &buffered[buffered_idx];
  buffered_idx += 16;

  // Create UUID from Truly Random or Pseudo-Random Numbers.
  // See: https://tools.ietf.org/html/rfc4122#section-4.4
  rand[6] = (rand[6] & 0x0f) | 0x40;  // UUID version 4 (random)
  rand[8] = (rand[8] & 0x3f) | 0x80;  // UUID variant 1 (RFC4122)

  // Convert UUID to a string representation, e.g.
  // a121e9e1-feae-4136-9e0e-6fac343d56c9.
  static const char* const hex = "0123456789abcdef";
  char uuid[UUID_LENGTH];

  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t d = rand[i];
    uuid[2 * i] = hex[d >> 4];
    uuid[2 * i + 1] = hex[d & 0x0f];
  }

  uuid[8] = '-';

  for (uint8_t i = 4; i < 6; i++) {
    const uint8_t d = rand[i];
    uuid[2 * i + 1] = hex[d >> 4];
    uuid[2 * i + 2] = hex[d & 0x0f];
  }

  uuid[13] = '-';

  for (uint8_t i = 6; i < 8; i++) {
    const uint8_t d = rand[i];
    uuid[2 * i + 2] = hex[d >> 4];
    uuid[2 * i + 3] = hex[d & 0x0f];
  }

  uuid[18] = '-';

  for (uint8_t i = 8; i < 10; i++) {
    const uint8_t d = rand[i];
    uuid[2 * i + 3] = hex[d >> 4];
    uuid[2 * i + 4] = hex[d & 0x0f];
  }

  uuid[23] = '-';

  for (uint8_t i = 10; i < 16; i++) {
    const uint8_t d = rand[i];
    uuid[2 * i + 4] = hex[d >> 4];
    uuid[2 * i + 5] = hex[d & 0x0f];
  }

  return std::string(uuid, UUID_LENGTH);
}

void RandomGeneratorImpl::randomBuffer(char* ch, size_t len) {
  std::random_device engine;
  std::uniform_int_distribution<std::size_t> dist(0, CHARS.size() - 1);
  for (size_t i = 0; i < len; ++i) {
    ch[i] = CHARS[dist(engine)];
  }
}

}  // namespace cpp2sky
