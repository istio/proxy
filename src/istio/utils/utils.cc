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

#include "src/istio/utils/utils.h"

#include <sstream>
#include <vector>

namespace istio {
namespace utils {

namespace {
const std::string kNamespaceKey("ns");
const char kDelimiter = '/';
}  // namespace

bool GetSourceNamespace(const std::string& principal,
                        std::string* source_namespace) {
  if (source_namespace) {
    // The namespace is a substring in principal with format:
    // "<DOMAIN>/ns/<NAMESPACE>/sa/<SERVICE-ACCOUNT>". '/' is not allowed to
    // appear in actual content except as delimiter between tokens.
    // The following algorithm extracts the namespace based on the above format
    // but is a little more flexible. It assumes that the principal begins with
    // a <DOMAIN> string followed by <key,value> pairs separated by '/'.

    // Skip the first <DOMAIN> string and check remaining <key, value> pairs.
    size_t begin = principal.find(kDelimiter);
    if (begin == std::string::npos) {
      return false;
    }
    begin += 1;

    while (1) {
      size_t key_end = principal.find(kDelimiter, begin);
      if (key_end == std::string::npos) {
        return false;
      }

      size_t value_begin = key_end + 1;
      size_t value_end = principal.find(kDelimiter, value_begin);

      if (principal.compare(begin, key_end - begin, kNamespaceKey) == 0) {
        size_t len = (value_end == std::string::npos ? value_end
                                                     : value_end - value_begin);
        *source_namespace = principal.substr(value_begin, len);
        return true;
      }

      if (value_end == std::string::npos) {
        return false;
      }

      begin = value_end + 1;
    }
  }
  return false;
}

}  // namespace utils
}  // namespace istio
