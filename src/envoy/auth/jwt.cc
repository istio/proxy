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

#include "jwt.h"

#include "common/common/base64.h"
#include "common/common/utility.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "rapidjson/document.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

namespace Base64url {

std::string decode(std::string input) {
  /*
   * TODO: check input validity
   */
  /*
   * base64url is using '-', '_' instead of '+', '/' in base64 string.
   */
  std::replace(input.begin(), input.end(), '-', '+');
  std::replace(input.begin(), input.end(), '_', '/');
  /*
   * base64 string should be padded with '=' so as to the length of the string
   * is divisible by 4.
   */
  switch (input.length() % 4) {
    case 0:
      break;
    case 2:
      input += "==";
      break;
    case 3:
      input += "=";
      break;
    default:
      /*
       * for invalid base64url input, return empty string
       */
      return "";
  }
  return Base64::decode(input);
}

}  // namespace Base64url

namespace Util {

const uint8_t *castToUChar(const std::string &str) {
  return reinterpret_cast<const uint8_t *>(str.c_str());
}

}  // namespace Util

bssl::UniquePtr<EVP_PKEY> evpPkeyFromStr(const std::string &pkey_pem) {
  std::string pkey_der = Base64::decode(pkey_pem);
  bssl::UniquePtr<RSA> rsa(RSA_public_key_from_bytes(
      Util::castToUChar(pkey_der), pkey_der.length()));
  if (rsa == nullptr) {
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
  EVP_PKEY_set1_RSA(key.get(), rsa.get());

  return key;
}

const EVP_MD *evpMdFromAlg(const std::string &alg) {
  /*
   * may use
   * EVP_sha384() if alg == "RS384" and
   * EVP_sha512() if alg == "RS512"
   */
  if (alg == "RS256") {
    return EVP_sha256();
  } else {
    return nullptr;
  }
}

bool verifySignature(bssl::UniquePtr<EVP_PKEY> key, const std::string &alg,
                     const uint8_t *signature, size_t signature_len,
                     const uint8_t *signed_data, size_t signed_data_len) {
  bssl::UniquePtr<EVP_MD_CTX> md_ctx(EVP_MD_CTX_create());
  const EVP_MD *md = evpMdFromAlg(alg);

  if (md == nullptr) {
    return false;
  }
  if (md_ctx == nullptr) {
    return false;
  }
  if (EVP_DigestVerifyInit(md_ctx.get(), nullptr, md, nullptr, key.get()) !=
      1) {
    return false;
  }
  if (EVP_DigestVerifyUpdate(md_ctx.get(), signed_data, signed_data_len) != 1) {
    return false;
  }
  if (EVP_DigestVerifyFinal(md_ctx.get(), signature, signature_len) != 1) {
    return false;
  }
  return true;
}

bool verifySignature(const std::string &pkey_pem, const std::string &alg,
                     const std::string &signature,
                     const std::string &signed_data) {
  return verifySignature(evpPkeyFromStr(pkey_pem), alg,
                         Util::castToUChar(signature), signature.length(),
                         Util::castToUChar(signed_data), signed_data.length());
}

}  // namespace

namespace Jwt {

std::unique_ptr<rapidjson::Document> decode(const std::string &jwt,
                                            const std::string &pkey_pem) {
  /*
   * TODO: return failure reason (something like
   * https://github.com/grpc/grpc/blob/master/src/core/lib/security/credentials/jwt/jwt_verifier.h#L38)
   */

  /*
   * TODO: support jwk
   */

  /*
   * jwt must have exactly 2 dots
   */
  if (std::count(jwt.begin(), jwt.end(), '.') != 2) {
    return nullptr;
  }
  std::vector<std::string> jwt_split = StringUtil::split(jwt, '.');
  if (jwt_split.size() != 3) {
    return nullptr;
  }
  const std::string &header_base64url_encoded = jwt_split[0];
  const std::string &payload_base64url_encoded = jwt_split[1];
  const std::string &signature_base64url_encoded = jwt_split[2];
  std::string signed_data = jwt_split[0] + '.' + jwt_split[1];

  /*
   * verification
   */
  rapidjson::Document header_json;
  if (header_json.Parse(Base64url::decode(header_base64url_encoded).c_str())
          .HasParseError()) {
    return nullptr;
  }

  if (!header_json.HasMember("alg")) {
    return nullptr;
  }
  rapidjson::Value &alg_v = header_json["alg"];
  if (!alg_v.IsString()) {
    return nullptr;
  }
  std::string alg = alg_v.GetString();

  std::string signature = Base64url::decode(signature_base64url_encoded);
  bool valid = verifySignature(pkey_pem, alg, signature, signed_data);

  // if signature is invalid, it will not decode the payload
  if (!valid) {
    return nullptr;
  }

  /*
   * decode payload
   */
  std::unique_ptr<rapidjson::Document> payload_json_ptr(
      new rapidjson::Document());
  if (payload_json_ptr
          ->Parse(Base64url::decode(payload_base64url_encoded).c_str())
          .HasParseError()) {
    return nullptr;
  }

  return payload_json_ptr;
};

}  // Jwt
}  // Auth
}  // Http
}  // Envoy