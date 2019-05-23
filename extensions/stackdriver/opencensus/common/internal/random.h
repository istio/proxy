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

#ifndef OPENCENSUS_COMMON_INTERNAL_RANDOM_H_
#define OPENCENSUS_COMMON_INTERNAL_RANDOM_H_

#include <cstddef>
#include <cstdint>
#include <random>

#ifndef NULL_PLUGIN
#include "api/wasm/cpp/proxy_wasm_intrinsics.h"
#else
#include "extensions/common/wasm/null/null.h"
using namespace Envoy::Extensions::Common::Wasm::Null::Plugin;
#endif

namespace opencensus {
namespace common {

class Generator {
 public:
  Generator() : rng_(proxy_getCurrentTimeNanoseconds()) {}
  explicit Generator(uint64_t seed) : rng_(seed) {}

  uint64_t Random64();

 private:
  friend class Random;

  std::mt19937_64 rng_;
};

class Random {
 public:
  // Initializes and returns a singleton Random generator.
  static Random* GetRandom();

  // Generating functions.
  // Generates a random uint32_t
  uint32_t GenerateRandom32();
  // Generates a random uint64_t
  uint64_t GenerateRandom64();
  // Generates a random float between [0.0, 1.0]
  float GenerateRandomFloat();
  // Generates a random double between [0.0, 1.0]
  double GenerateRandomDouble();
  // Fills the given buffer with uniformly random bits.
  void GenerateRandomBuffer(uint8_t* buf, size_t buf_size);

 private:
  Random() = default;

  Random(const Random&) = delete;
  Random(Random&&) = delete;
  Random& operator=(const Random&) = delete;
  Random& operator=(Random&&) = delete;

  uint64_t GenerateValue();
  Generator gen_;
};

}  // namespace common
}  // namespace opencensus

#endif  // OPENCENSUS_COMMON_INTERNAL_RANDOM_H_
