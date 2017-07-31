//
// Created by mtakigiku on 7/27/17.
//

#include "jwt.h"

#include "common/common/base64.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "rapidjson/document.h"

//#include <boost/algorithm/string.hpp>

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

uint8_t* unsigned_c_str(const std::string& str) {
  return reinterpret_cast<uint8_t*>(const_cast<char*>(str.c_str()));
}

std::vector<std::string> split(std::string str, char delimiter) {
  std::vector<std::string> internal;
  std::stringstream ss(str);
  std::string tok;

  while (std::getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }
  return internal;
}

}  // namespace Util

EVP_PKEY* Jwt::evp_pkey_from_str(const std::string& pkey_pem) {
  std::string pkey_der = Base64::decode(pkey_pem);
  RSA* rsa = RSA_public_key_from_bytes(Util::unsigned_c_str(pkey_der),
                                       pkey_der.length());
  if (rsa == NULL) {
    return NULL;
  }
  EVP_PKEY* key = EVP_PKEY_new();
  EVP_PKEY_set1_RSA(key, rsa);
  RSA_free(rsa);

  return key;
}

const EVP_MD* Jwt::evp_md_from_alg(const std::string& alg) {
  /*
   * may use
   * EVP_sha384() if alg == "RS384" and
   * EVP_sha512() if alg == "RS512"
   */
  if (alg == "RS256") {
    return EVP_sha256();
  } else {
    return NULL;
  }
}

bool Jwt::verify_signature(EVP_PKEY* key, const std::string& alg,
                           uint8_t* signature, size_t signature_len,
                           uint8_t* signed_data, size_t signed_data_len) {
  EVP_MD_CTX* md_ctx = EVP_MD_CTX_create();

  const EVP_MD* md = evp_md_from_alg(alg);

  bool result = false;

  assert(md != NULL);
  if (md_ctx == NULL) {
    goto end;
  }
  if (EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, key) != 1) {
    goto end;
  }
  if (EVP_DigestVerifyUpdate(md_ctx, signed_data, signed_data_len) != 1) {
    goto end;
  }
  if (EVP_DigestVerifyFinal(md_ctx, signature, signature_len) != 1) {
    goto end;
  }
  result = true;

end:
  if (md_ctx != NULL) {
    EVP_MD_CTX_destroy(md_ctx);
  }
  return result;
}

bool Jwt::verify_signature(const std::string& pkey_pem, const std::string& alg,
                           const std::string signature,
                           const std::string& signed_data) {
  EVP_PKEY* key = evp_pkey_from_str(pkey_pem);

  bool valid = verify_signature(
      key, alg, Util::unsigned_c_str(signature), signature.length(),
      Util::unsigned_c_str(signed_data), signed_data.length());

  if (key != NULL) EVP_PKEY_free(key);

  return valid;
}

std::pair<bool, rapidjson::Document*> Jwt::decode(const std::string& jwt,
                                                  const std::string& pkey_pem) {
  // return value in failure cases
  std::pair<bool, rapidjson::Document*> failed =
      std::make_pair(false, (rapidjson::Document*)NULL);

  std::vector<std::string> jwt_split = Util::split(jwt, '.');
  if (jwt_split.size() != 3) {
    return failed;
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
    return failed;
  }
  rapidjson::Value& alg_v = header_json["alg"];
  if (!alg_v.IsString()) {
    return failed;
  }
  std::string alg = alg_v.GetString();

  std::string signature = Base64url::decode(signature_base64url_encoded);
  bool valid = verify_signature(pkey_pem, alg, signature, signed_data);

  /*
   * decode payload
   */
  rapidjson::Document* payload_json = new rapidjson::Document();
  payload_json->Parse(Base64url::decode(payload_base64url_encoded).c_str());

  return std::make_pair(valid, payload_json);
};

}  // Auth
}  // Http
}  // Envoy