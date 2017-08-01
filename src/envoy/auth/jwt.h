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

#ifndef PROXY_JWT_H
#define PROXY_JWT_H

#include "openssl/evp.h"
#include "rapidjson/document.h"

#include <string>
#include <utility>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

class Jwt {
 public:
  static std::unique_ptr<rapidjson::Document> decode(
      const std::string& jwt, const std::string& pkey_pem);

 private:
  static bssl::UniquePtr<EVP_PKEY> evpPkeyFromStr(const std::string& pkey_pem);
  static const EVP_MD* evpMdFromAlg(const std::string& alg);
  static bool verifySignature(bssl::UniquePtr<EVP_PKEY> key,
                              const std::string& alg, const uint8_t* signature,
                              size_t signature_len, const uint8_t* signed_data,
                              size_t signed_data_len);
  static bool verifySignature(const std::string& pkey_pem,
                              const std::string& alg,
                              const std::string& signature,
                              const std::string& signed_data);
};

}  // Auth
}  // Http
}  // Envoy

#endif  // PROXY_JWT_H
