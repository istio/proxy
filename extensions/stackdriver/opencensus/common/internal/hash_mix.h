// Copyright 2018, OpenCensus Authors
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

#ifndef OPENCENSUS_COMMON_INTERNAL_HASH_MIX_H_
#define OPENCENSUS_COMMON_INTERNAL_HASH_MIX_H_

#include <cstddef>
#include <limits>

namespace opencensus {
namespace common {

// HashMix provides efficient mixing of hash values.
class HashMix final {
 public:
  HashMix() : hash_(1) {}

  // Mixes in another *hashed* value.
  void Mix(std::size_t hash) {
    // A multiplier that has been found to provide good mixing.
    constexpr std::size_t kMul =
        static_cast<std::size_t>(0xdc3eb94af8ab4c93ULL);
    hash_ *= kMul;
    hash_ =
        ((hash << 19) | (hash >> (std::numeric_limits<size_t>::digits - 19))) +
        hash;
  }

  size_t get() const { return hash_; }

 private:
  std::size_t hash_;
};

}  // namespace common
}  // namespace opencensus

#endif  // OPENCENSUS_COMMON_INTERNAL_HASH_MIX_H_
