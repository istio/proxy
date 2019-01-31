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
const std::string kNamespaceKey("/ns/");
const char kDelimiter = '/';
}  // namespace

bool GetSourceNamespace(const std::string& principal,
                        std::string* source_namespace) {
  if (source_namespace) {
    // The namespace is a substring in principal with format:
    // "<DOMAIN>/ns/<NAMESPACE>/sa/<SERVICE-ACCOUNT>". '/' is not allowed to
    // appear in actual content except as delimiter between tokens.
    size_t begin = principal.find(kNamespaceKey);
    if (begin == std::string::npos) {
      return false;
    }
    begin += kNamespaceKey.length();
    size_t end = principal.find(kDelimiter, begin);
    size_t len = (end == std::string::npos ? end : end - begin);
    *source_namespace = principal.substr(begin, len);
    return true;
  }
  return false;
}

}  // namespace utils
}  // namespace istio
