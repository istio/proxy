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

#ifndef ISTIO_UTILS_CONCAT_HASH_H_
#define ISTIO_UTILS_CONCAT_HASH_H_

#include <string.h>

#include <functional>
#include <string>

namespace istio {
namespace utils {

// The hash type for Check cache.
typedef std::size_t HashType;

// This class concatenates multiple values into a string as hash
class ConcatHash {
 public:
  ConcatHash(size_t reserve_size) { hash_.reserve(reserve_size); }

  // Updates the context with data.
  ConcatHash& Update(const void* data, size_t size) {
    hash_.append(static_cast<const char*>(data), size);
    return *this;
  }

  // A helper function for int
  ConcatHash& Update(int d) { return Update(&d, sizeof(d)); }

  // A helper function for const char*
  ConcatHash& Update(const char* str) {
    hash_.append(str);
    return *this;
  }

  // A helper function for const string
  ConcatHash& Update(const std::string& str) {
    hash_.append(str);
    return *this;
  }

  // Returns the hash of the concated string.
  HashType getHash() const { return std::hash<std::string>{}(hash_); }

 private:
  std::string hash_;
};

}  // namespace utils
}  // namespace istio

#endif  // ISTIO_UTILS_CONCAT_HASH_H_
