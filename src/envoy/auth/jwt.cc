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
#include "openssl/bn.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "rapidjson/document.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

std::string StatusToString(Status status) {
  static std::map<Status, std::string> table = {
      {Status::OK, "OK"},
      {Status::JWT_BAD_FORMAT, "JWT_BAD_FORMAT"},
      {Status::JWT_HEADER_PARSE_ERROR, "JWT_HEADER_PARSE_ERROR"},
      {Status::JWT_HEADER_NO_ALG, "JWT_HEADER_NO_ALG"},
      {Status::JWT_HEADER_BAD_ALG, "JWT_HEADER_BAD_ALG"},
      {Status::JWT_SIGNATURE_PARSE_ERROR, "JWT_SIGNATURE_PARSE_ERROR"},
      {Status::JWT_INVALID_SIGNATURE, "JWT_INVALID_SIGNATURE"},
      {Status::JWT_PAYLOAD_PARSE_ERROR, "JWT_PAYLOAD_PARSE_ERROR"},
      {Status::JWT_HEADER_BAD_KID, "JWT_HEADER_BAD_KID"},
      {Status::JWK_PARSE_ERROR, "JWK_PARSE_ERROR"},
      {Status::JWK_NO_KEYS, "JWK_NO_KEYS"},
      {Status::JWK_BAD_KEYS, "JWK_BAD_KEYS"},
      {Status::JWK_NO_VALID_PUBKEY, "JWK_NO_VALID_PUBKEY"},
      {Status::KID_UNMATCH, "KID_UNMATCH"},
      {Status::ALG_NOT_IMPLEMENTED, "ALG_NOT_IMPLEMENTED"},
      {Status::PUBKEY_PEM_BAD_FORMAT, "PUBKEY_PEM_BAD_FORMAT"},
      {Status::PUBKEY_RSA_OBJECT_NULL, "PUBKEY_RSA_OBJECT_NULL"},
      {Status::EVP_MD_CTX_CREATE_FAIL, "EVP_MD_CTX_CREATE_FAIL"},
      {Status::DIGEST_VERIFY_INIT_FAIL, "DIGEST_VERIFY_INIT_FAIL"},
      {Status::DIGEST_VERIFY_UPDATE_FAIL, "DIGEST_VERIFY_UPDATE_FAIL"}};
  return table[status];
}

namespace {

// Conversion table is taken from
// https://opensource.apple.com/source/QuickTimeStreamingServer/QuickTimeStreamingServer-452/CommonUtilitiesLib/base64.c
//
// and modified the position of 62 ('+' to '-') and 63 ('/' to '_')
const uint8_t kReverseLookupTableBase64Url[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 64, 64, 64, 64, 64, 64, 64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64,
    63, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64};

bool IsNotBase64UrlChar(int8_t c) {
  return kReverseLookupTableBase64Url[static_cast<int32_t>(c)] & 64;
}

std::string Base64UrlDecode(std::string input) {
  // allow at most 2 padding letters at the end of the input, only if input
  // length is divisible by 4
  int len = input.length();
  if (len % 4 == 0) {
    if (input[len - 1] == '=') {
      input.pop_back();
      if (input[len - 2] == '=') {
        input.pop_back();
      }
    }
  }
  // if input contains non-base64url character, return empty string
  // Note: padding letter must not be contained
  if (std::find_if(input.begin(), input.end(), IsNotBase64UrlChar) !=
      input.end()) {
    return "";
  }

  // base64url is using '-', '_' instead of '+', '/' in base64 string.
  std::replace(input.begin(), input.end(), '-', '+');
  std::replace(input.begin(), input.end(), '_', '/');

  // base64 string should be padded with '=' so as to the length of the string
  // is divisible by 4.
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
      // * an invalid base64url input. return empty string.
      return "";
  }
  return Base64::decode(input);
}

const uint8_t *CastToUChar(const std::string &str) {
  return reinterpret_cast<const uint8_t *>(str.c_str());
}

// Class to create EVP_PKEY object from string of public key, formatted in PEM
// or JWKs.
// If it failed, status_ holds the failure reason.
//
// Usage example:
//   EvpPkeyGetter e;
//   bssl::UniquePtr<EVP_PKEY> pkey =
//   e.EvpPkeyFromStr(pem_formatted_public_key);
// (You can use EvpPkeyFromJwk() for JWKs)
class EvpPkeyGetter {
 public:
  EvpPkeyGetter() : status_(Status::OK) {}

  // It returns "OK" or the failure reason.
  Status GetStatus() { return status_; }

  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromStr(const std::string &pkey_pem) {
    std::string pkey_der = Base64::decode(pkey_pem);
    if (pkey_der == "") {
      UpdateStatus(Status::PUBKEY_PEM_BAD_FORMAT);
      return nullptr;
    }
    return EvpPkeyFromRsa(
        bssl::UniquePtr<RSA>(
            RSA_public_key_from_bytes(CastToUChar(pkey_der), pkey_der.length()))
            .get());
  }

  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromJwk(const std::string &n,
                                           const std::string &e) {
    return EvpPkeyFromRsa(RsaFromJwk(n, e).get());
  }

 private:
  // It holds failure reason.
  Status status_;

  void UpdateStatus(Status status) {
    if (status_ == Status::OK) {
      status_ = status;
    }
  }

  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromRsa(RSA *rsa) {
    if (!rsa) {
      UpdateStatus(Status::PUBKEY_RSA_OBJECT_NULL);
      return nullptr;
    }
    bssl::UniquePtr<EVP_PKEY> key(EVP_PKEY_new());
    EVP_PKEY_set1_RSA(key.get(), rsa);
    return key;
  }

  bssl::UniquePtr<BIGNUM> BigNumFromBase64UrlString(const std::string &s) {
    std::string s_decoded = Base64UrlDecode(s);
    if (s_decoded == "") {
      return nullptr;
    }
    return bssl::UniquePtr<BIGNUM>(
        BN_bin2bn(CastToUChar(s_decoded), s_decoded.length(), NULL));
  };

  bssl::UniquePtr<RSA> RsaFromJwk(const std::string &n, const std::string &e) {
    bssl::UniquePtr<RSA> rsa(RSA_new());
    if (!rsa) {
      // Couldn't create RSA key.
      status_ = Status::PUBKEY_RSA_OBJECT_NULL;
      return nullptr;
    }
    rsa->n = BigNumFromBase64UrlString(n).release();
    rsa->e = BigNumFromBase64UrlString(e).release();
    if (!rsa->n || !rsa->e) {
      // RSA public key field is missing.
      return nullptr;
    }
    return rsa;
  }
};

// Class to decode and verify JWT. Setup() must be called before
// VerifySignature() and Payload(). If you do not need the signature
// verification, VerifySignature() can be skipped.
// When verification fails, status_ holds the reason of failure.
//
// Usage example:
//   Verifier v;
//   if(!v.Setup(jwt)) return nullptr;
//   if(!v.VerifySignature(publickey)) return nullptr;
//   return v.Payload();
class Verifier {
 public:
  Verifier() : status_(Status::OK) {}

  // Returns the parsed header. Setup() must be called before this.
  rapidjson::Document &Header() { return header_; }

  // Returns "alg" in the header. Setup() must be called before this.
  std::string &Alg() { return alg_; };

  // Returns "OK" or the failure reason.
  Status GetStatus() { return status_; }

  // Parses header JSON. This function must be called before calling Header() or
  // Alg().
  // It returns false if parse fails.
  bool Setup(const std::string &jwt) {
    // jwt must have exactly 2 dots
    if (std::count(jwt.begin(), jwt.end(), '.') != 2) {
      UpdateStatus(Status::JWT_BAD_FORMAT);
      return false;
    }
    jwt_split = StringUtil::split(jwt, '.');
    if (jwt_split.size() != 3) {
      UpdateStatus(Status::JWT_BAD_FORMAT);
      return false;
    }

    // parse header json
    if (header_.Parse(Base64UrlDecode(jwt_split[0]).c_str()).HasParseError()) {
      UpdateStatus(Status::JWT_HEADER_PARSE_ERROR);
      return false;
    }

    if (!header_.HasMember("alg")) {
      UpdateStatus(Status::JWT_HEADER_NO_ALG);
      return false;
    }
    rapidjson::Value &alg_v = header_["alg"];
    if (!alg_v.IsString()) {
      UpdateStatus(Status::JWT_HEADER_BAD_ALG);
      return false;
    }
    alg_ = alg_v.GetString();

    return true;
  }

  // Setup() must be called before VerifySignature().
  bool VerifySignature(EVP_PKEY *key) {
    std::string signature = Base64UrlDecode(jwt_split[2]);
    if (signature == "") {
      // Signature is a bad Base64url input.
      UpdateStatus(Status::JWT_SIGNATURE_PARSE_ERROR);
      return false;
    }
    std::string signed_data = jwt_split[0] + '.' + jwt_split[1];
    if (!VerifySignature(key, alg_, signature, signed_data)) {
      UpdateStatus(Status::JWT_INVALID_SIGNATURE);
      return false;
    }
    return true;
  }

  // Returns payload JSON.
  // VerifySignature() must be called before Payload().
  std::unique_ptr<rapidjson::Document> Payload() {
    // decode payload
    std::unique_ptr<rapidjson::Document> payload_json(
        new rapidjson::Document());
    if (payload_json->Parse(Base64UrlDecode(jwt_split[1]).c_str())
            .HasParseError()) {
      UpdateStatus(Status::JWT_PAYLOAD_PARSE_ERROR);
      return nullptr;
    }
    return payload_json;
  }

 private:
  std::vector<std::string> jwt_split;
  rapidjson::Document header_;
  std::string alg_;
  Status status_;

  // Not overwrite failure status to keep the reason of the first failure
  void UpdateStatus(Status status) {
    if (status_ == Status::OK) {
      status_ = status;
    }
  }

  const EVP_MD *EvpMdFromAlg(const std::string &alg) {
    // may use
    // EVP_sha384() if alg == "RS384" and
    // EVP_sha512() if alg == "RS512"
    if (alg == "RS256") {
      return EVP_sha256();
    } else {
      return nullptr;
    }
  }

  bool VerifySignature(EVP_PKEY *key, const std::string &alg,
                       const uint8_t *signature, size_t signature_len,
                       const uint8_t *signed_data, size_t signed_data_len) {
    bssl::UniquePtr<EVP_MD_CTX> md_ctx(EVP_MD_CTX_create());
    const EVP_MD *md = EvpMdFromAlg(alg);

    if (!md) {
      UpdateStatus(Status::ALG_NOT_IMPLEMENTED);
      return false;
    }
    if (!md_ctx) {
      UpdateStatus(Status::EVP_MD_CTX_CREATE_FAIL);
      return false;
    }
    if (EVP_DigestVerifyInit(md_ctx.get(), nullptr, md, nullptr, key) != 1) {
      UpdateStatus(Status::DIGEST_VERIFY_INIT_FAIL);
      return false;
    }
    if (EVP_DigestVerifyUpdate(md_ctx.get(), signed_data, signed_data_len) !=
        1) {
      UpdateStatus(Status::DIGEST_VERIFY_UPDATE_FAIL);
      return false;
    }
    if (EVP_DigestVerifyFinal(md_ctx.get(), signature, signature_len) != 1) {
      UpdateStatus(Status::JWT_INVALID_SIGNATURE);
      return false;
    }
    return true;
  }

  bool VerifySignature(EVP_PKEY *key, const std::string &alg,
                       const std::string &signature,
                       const std::string &signed_data) {
    return VerifySignature(key, alg, CastToUChar(signature), signature.length(),
                           CastToUChar(signed_data), signed_data.length());
  }
};

}  // namespace

void JwtVerifier::UpdateStatus(Status status) {
  // Not overwrite failure status to keep the reason of the first failure
  if (status_ == Status::OK) {
    status_ = status;
  }
}

JwtVerifierPem &JwtVerifierPem::SetPublicKey(const std::string &pkey_pem) {
  EvpPkeyGetter e;
  pkey_ = e.EvpPkeyFromStr(pkey_pem);
  UpdateStatus(e.GetStatus());
  return *this;
}

std::unique_ptr<rapidjson::Document> JwtVerifierPem::Decode(
    const std::string &jwt) {
  Verifier v;
  auto payload = pkey_ && v.Setup(jwt) && v.VerifySignature(pkey_.get())
                     ? v.Payload()
                     : nullptr;
  UpdateStatus(v.GetStatus());
  return payload;
}

JwtVerifierJwks &JwtVerifierJwks::SetPublicKey(const std::string &pkey_jwks) {
  rapidjson::Document jwks_json;
  if (jwks_json.Parse(pkey_jwks.c_str()).HasParseError()) {
    UpdateStatus(Status::JWK_PARSE_ERROR);
    return *this;
  }
  auto keys = jwks_json.FindMember("keys");
  if (keys == jwks_json.MemberEnd()) {
    UpdateStatus(Status::JWK_NO_KEYS);
    return *this;
  }
  if (!keys->value.IsArray()) {
    UpdateStatus(Status::JWK_BAD_KEYS);
    return *this;
  }

  for (auto &jwk_json : keys->value.GetArray()) {
    std::unique_ptr<Jwk> jwk(new Jwk());

    if (!jwk_json.HasMember("kid") || !jwk_json["kid"].IsString()) {
      continue;
    }
    jwk->kid_ = jwk_json["kid"].GetString();

    if (!jwk_json.HasMember("alg") || !jwk_json["alg"].IsString()) {
      continue;
    }
    jwk->alg_ = jwk_json["alg"].GetString();

    // public key
    if (!jwk_json.HasMember("n") || !jwk_json["n"].IsString()) {
      continue;
    }
    if (!jwk_json.HasMember("e") || !jwk_json["e"].IsString()) {
      continue;
    }
    EvpPkeyGetter e;
    jwk->pkey_ =
        e.EvpPkeyFromJwk(jwk_json["n"].GetString(), jwk_json["e"].GetString());

    jwks_.push_back(std::move(jwk));
  }
  if (jwks_.size() == 0) {
    UpdateStatus(Status::JWK_NO_VALID_PUBKEY);
  }
  return *this;
}

std::unique_ptr<rapidjson::Document> JwtVerifierJwks::Decode(
    const std::string &jwt) {
  Verifier v;
  if (!v.Setup(jwt)) {
    UpdateStatus(v.GetStatus());
    return nullptr;
  }
  std::string kid_jwt = "";
  if (v.Header().HasMember("kid")) {
    if (v.Header()["kid"].IsString()) {
      kid_jwt = v.Header()["kid"].GetString();
    } else {
      // if header has invalid format (non-string) "kid", verification is
      // considered to be failed
      UpdateStatus(Status::JWT_HEADER_BAD_KID);
      return nullptr;
    }
  }

  bool kid_matched = false;
  for (auto &jwk : jwks_) {
    // If kid is specified in JWT, JWK with the same kid is used for
    // verification.
    // If kid is not specified in JWT, try all JWK.
    if (kid_jwt != "" && jwk->kid_ != kid_jwt) {
      continue;
    }
    kid_matched = true;

    // The same alg must be used.
    if (jwk->alg_ != v.Alg()) {
      continue;
    }

    if (v.VerifySignature(jwk->pkey_.get())) {
      return v.Payload();
    }
  }

  if (kid_matched) {
    UpdateStatus(Status::JWT_INVALID_SIGNATURE);
  } else {
    UpdateStatus(Status::KID_UNMATCH);
  }
  return nullptr;
}

}  // Auth
}  // Http
}  // Envoy