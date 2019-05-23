// Copyright 2017, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "opencensus/common/internal/random.h"

#include <cstring>

namespace opencensus {
namespace common {

uint64_t Generator::Random64() { return rng_(); }

Random* Random::GetRandom() {
  static auto* const global_random = new Random;
  return global_random;
}

uint32_t Random::GenerateRandom32() { return gen_.Random64(); }

uint64_t Random::GenerateRandom64() { return gen_.Random64(); }

float Random::GenerateRandomFloat() {
  return static_cast<float>(gen_.Random64()) / static_cast<float>(UINT64_MAX);
}

double Random::GenerateRandomDouble() {
  return static_cast<double>(gen_.Random64()) / static_cast<double>(UINT64_MAX);
}

void Random::GenerateRandomBuffer(uint8_t* buf, size_t buf_size) {
  for (size_t i = 0; i < buf_size; i += sizeof(uint64_t)) {
    uint64_t value = gen_.rng_();
    if (i + sizeof(uint64_t) <= buf_size) {
      memcpy(&buf[i], &value, sizeof(uint64_t));
    } else {
      memcpy(&buf[i], &value, buf_size - i);
    }
  }
}

}  // namespace common
}  // namespace opencensus
