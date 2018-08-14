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

    // Spilt the principal into tokens separated by kDelimiter.
    std::stringstream ss(principal);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(ss, token, kDelimiter)) {
      tokens.push_back(token);
    }

    // Skip the first <DOMAIN> string and check remaining <key, value> pairs.
    for (int i = 1; i < tokens.size(); i += 2) {
      if (tokens[i] == kNamespaceKey) {
        // Found the namespace key, treat the following token as namespace.
        int j = i + 1;
        *source_namespace = (j < tokens.size() ? tokens[j] : "");
        return true;
      }
    }
  }
  return false;
}

}  // namespace utils
}  // namespace istio
