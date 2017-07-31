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

namespace Base64url {

std::string decode(std::string input) {
  std::replace(input.begin(), input.end(), '-', '+');
  std::replace(input.begin(), input.end(), '_', '/');
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
      /* invalid base64url input*/
      assert(0);
  }
  return Base64::decode(input);
}

}  // namespace Base64url

namespace Util {

const uint8_t* unsignedCStr(const std::string& str) {
  return reinterpret_cast<const uint8_t*>(str.c_str());
}

}  // namespace Util

bssl::UniquePtr<EVP_PKEY> Jwt::evpPkeyFromStr(const std::string& pkey_pem) {
  std::string pkey_der = Base64::decode(pkey_pem);
  bssl::UniquePtr<RSA> rsa(RSA_public_key_from_bytes(
      Util::unsignedCStr(pkey_der), pkey_der.length()));
  if (rsa == nullptr) {
    return nullptr;
  }
  bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
  EVP_PKEY_set1_RSA(key.get(), rsa.get());

  return key;
}

const EVP_MD* Jwt::evpMdFromAlg(const std::string& alg) {
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

bool Jwt::verifySignature(bssl::UniquePtr<EVP_PKEY> key, const std::string& alg,
                          const uint8_t* signature, size_t signature_len,
                          const uint8_t* signed_data, size_t signed_data_len) {
  bssl::UniquePtr<EVP_MD_CTX> md_ctx(EVP_MD_CTX_create());
  const EVP_MD* md = evpMdFromAlg(alg);

  assert(md != nullptr);
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

bool Jwt::verifySignature(const std::string& pkey_pem, const std::string& alg,
                          const std::string& signature,
                          const std::string& signed_data) {
  return verifySignature(evpPkeyFromStr(pkey_pem), alg,
                         Util::unsignedCStr(signature), signature.length(),
                         Util::unsignedCStr(signed_data), signed_data.length());
}

std::unique_ptr<rapidjson::Document> Jwt::decode(const std::string& jwt,
                                                 const std::string& pkey_pem) {
  std::vector<std::string> jwt_split = StringUtil::split(jwt, '.');
  if (jwt_split.size() != 3) {
    return nullptr;
  }
  std::string header_base64url_encoded = jwt_split[0];
  std::string payload_base64url_encoded = jwt_split[1];
  std::string signature_base64url_encoded = jwt_split[2];
  std::string signed_data = jwt_split[0] + '.' + jwt_split[1];

  /*
   * verification
   */
  rapidjson::Document header_json;
  header_json.Parse(Base64url::decode(header_base64url_encoded).c_str());

  if (!header_json.HasMember("alg")) {
    return nullptr;
  }
  rapidjson::Value& alg_v = header_json["alg"];
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
  payload_json_ptr->Parse(Base64url::decode(payload_base64url_encoded).c_str());

  return payload_json_ptr;
};

}  // Auth
}  // Http
}  // Envoy