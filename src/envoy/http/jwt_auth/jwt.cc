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

#include <algorithm>
#include <cassert>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common/common/assert.h"
#include "common/common/base64.h"
#include "common/common/utility.h"
#include "openssl/bn.h"
#include "openssl/ecdsa.h"
#include "openssl/evp.h"
#include "openssl/rsa.h"
#include "openssl/sha.h"

namespace Envoy {
namespace Http {
namespace JwtAuth {

std::string StatusToString(Status status) {
  static std::map<Status, std::string> table = {
      {Status::OK, "OK"},
      {Status::JWT_MISSED, "Required JWT token is missing"},
      {Status::JWT_EXPIRED, "JWT is expired"},
      {Status::JWT_BAD_FORMAT, "JWT_BAD_FORMAT"},
      {Status::JWT_HEADER_PARSE_ERROR, "JWT_HEADER_PARSE_ERROR"},
      {Status::JWT_HEADER_NO_ALG, "JWT_HEADER_NO_ALG"},
      {Status::JWT_HEADER_BAD_ALG, "JWT_HEADER_BAD_ALG"},
      {Status::JWT_SIGNATURE_PARSE_ERROR, "JWT_SIGNATURE_PARSE_ERROR"},
      {Status::JWT_INVALID_SIGNATURE, "JWT_INVALID_SIGNATURE"},
      {Status::JWT_PAYLOAD_PARSE_ERROR, "JWT_PAYLOAD_PARSE_ERROR"},
      {Status::JWT_HEADER_BAD_KID, "JWT_HEADER_BAD_KID"},
      {Status::JWT_UNKNOWN_ISSUER, "Unknown issuer"},
      {Status::JWK_PARSE_ERROR, "JWK_PARSE_ERROR"},
      {Status::JWK_NO_KEYS, "JWK_NO_KEYS"},
      {Status::JWK_BAD_KEYS, "JWK_BAD_KEYS"},
      {Status::JWK_NO_VALID_PUBKEY, "JWK_NO_VALID_PUBKEY"},
      {Status::KID_ALG_UNMATCH, "KID_ALG_UNMATCH"},
      {Status::ALG_NOT_IMPLEMENTED, "ALG_NOT_IMPLEMENTED"},
      {Status::PEM_PUBKEY_BAD_BASE64, "PEM_PUBKEY_BAD_BASE64"},
      {Status::PEM_PUBKEY_PARSE_ERROR, "PEM_PUBKEY_PARSE_ERROR"},
      {Status::JWK_RSA_PUBKEY_PARSE_ERROR, "JWK_RSA_PUBKEY_PARSE_ERROR"},
      {Status::FAILED_CREATE_EC_KEY, "FAILED_CREATE_EC_KEY"},
      {Status::JWK_EC_PUBKEY_PARSE_ERROR, "JWK_EC_PUBKEY_PARSE_ERROR"},
      {Status::FAILED_CREATE_ECDSA_SIGNATURE, "FAILED_CREATE_ECDSA_SIGNATURE"},
      {Status::AUDIENCE_NOT_ALLOWED, "Audience doesn't match"},
      {Status::FAILED_FETCH_PUBKEY, "Failed to fetch public key"},
  };
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

}  // namespace

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

namespace {

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
// (You can use EvpPkeyFromJwkRSA() or EcKeyFromJwkEC() for JWKs)
class EvpPkeyGetter : public WithStatus {
 public:
  EvpPkeyGetter() {}

  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromStr(const std::string &pkey_pem) {
    std::string pkey_der = Base64::decode(pkey_pem);
    if (pkey_der == "") {
      UpdateStatus(Status::PEM_PUBKEY_BAD_BASE64);
      return nullptr;
    }
    auto rsa = bssl::UniquePtr<RSA>(
        RSA_public_key_from_bytes(CastToUChar(pkey_der), pkey_der.length()));
    if (!rsa) {
      UpdateStatus(Status::PEM_PUBKEY_PARSE_ERROR);
    }
    return EvpPkeyFromRsa(rsa.get());
  }

  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromJwkRSA(const std::string &n,
                                              const std::string &e) {
    return EvpPkeyFromRsa(RsaFromJwk(n, e).get());
  }

  bssl::UniquePtr<EC_KEY> EcKeyFromJwkEC(const std::string &x,
                                         const std::string &y) {
    bssl::UniquePtr<EC_KEY> ec_key(
        EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    if (!ec_key) {
      UpdateStatus(Status::FAILED_CREATE_EC_KEY);
      return nullptr;
    }
    bssl::UniquePtr<BIGNUM> bn_x = BigNumFromBase64UrlString(x);
    bssl::UniquePtr<BIGNUM> bn_y = BigNumFromBase64UrlString(y);
    if (!bn_x || !bn_y) {
      // EC public key field is missing or has parse error.
      UpdateStatus(Status::JWK_EC_PUBKEY_PARSE_ERROR);
      return nullptr;
    }

    if (EC_KEY_set_public_key_affine_coordinates(ec_key.get(), bn_x.get(),
                                                 bn_y.get()) == 0) {
      UpdateStatus(Status::JWK_EC_PUBKEY_PARSE_ERROR);
      return nullptr;
    }
    return ec_key;
  }

 private:
  // In the case where rsa is nullptr, UpdateStatus() should be called
  // appropriately elsewhere.
  bssl::UniquePtr<EVP_PKEY> EvpPkeyFromRsa(RSA *rsa) {
    if (!rsa) {
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
    // It crash if RSA object couldn't be created.
    assert(rsa);

    rsa->n = BigNumFromBase64UrlString(n).release();
    rsa->e = BigNumFromBase64UrlString(e).release();
    if (!rsa->n || !rsa->e) {
      // RSA public key field is missing or has parse error.
      UpdateStatus(Status::JWK_RSA_PUBKEY_PARSE_ERROR);
      return nullptr;
    }
    return rsa;
  }
};

}  // namespace

Jwt::Jwt(const std::string &jwt) {
  // jwt must have exactly 2 dots
  if (std::count(jwt.begin(), jwt.end(), '.') != 2) {
    UpdateStatus(Status::JWT_BAD_FORMAT);
    return;
  }
  auto jwt_split = StringUtil::splitToken(jwt, ".");
  if (jwt_split.size() != 3) {
    UpdateStatus(Status::JWT_BAD_FORMAT);
    return;
  }

  // Parse header json
  header_str_base64url_ = std::string(jwt_split[0].begin(), jwt_split[0].end());
  header_str_ = Base64UrlDecode(header_str_base64url_);

  auto result = Wasm::Common::JsonParse(header_str_);
  if (!result.has_value()) {
    UpdateStatus(Status::JWT_HEADER_PARSE_ERROR);
    return;
  }
  header_ = result.value();

  // Header should contain "alg".
  if (header_.find("alg") == header_.end()) {
    UpdateStatus(Status::JWT_HEADER_NO_ALG);
    return;
  }

  auto alg_field = Wasm::Common::JsonGetField<std::string>(header_, "alg");
  if (alg_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
    UpdateStatus(Status::JWT_HEADER_BAD_ALG);
    return;
  }
  alg_ = alg_field.value();

  if (alg_ != "RS256" && alg_ != "ES256" && alg_ != "RS384" &&
      alg_ != "RS512") {
    UpdateStatus(Status::ALG_NOT_IMPLEMENTED);
    return;
  }

  // Header may contain "kid", which should be a string if exists.
  auto kid_field = Wasm::Common::JsonGetField<std::string>(header_, "kid");
  if (kid_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
    if (kid_field.detail() ==
        Wasm::Common::JsonParserResultDetail::TYPE_ERROR) {
      UpdateStatus(Status::JWT_HEADER_BAD_KID);
      return;
    } else if (kid_field.detail() ==
               Wasm::Common::JsonParserResultDetail::OUT_OF_RANGE) {
      kid_ = "";
    } else {
      return;
    }
  } else {
    kid_ = kid_field.value();
  }

  // Parse payload json
  payload_str_base64url_ =
      std::string(jwt_split[1].begin(), jwt_split[1].end());
  payload_str_ = Base64UrlDecode(payload_str_base64url_);
  result = Wasm::Common::JsonParse(payload_str_);
  if (!result.has_value()) {
    UpdateStatus(Status::JWT_PAYLOAD_PARSE_ERROR);
    return;
  }
  payload_ = result.value();

  iss_ = Wasm::Common::JsonGetField<std::string>(payload_, "iss").value_or("");
  sub_ = Wasm::Common::JsonGetField<std::string>(payload_, "sub").value_or("");
  exp_ = Wasm::Common::JsonGetField<uint64_t>(payload_, "exp").value_or(0);

  // "aud" can be either string array or string.
  // Try as string array, read it as empty array if doesn't exist.
  if (!Wasm::Common::JsonArrayIterate(
          payload_, "aud", [&](const Wasm::Common::JsonObject &obj) -> bool {
            auto str_obj_result = Wasm::Common::JsonValueAs<std::string>(obj);
            if (str_obj_result.second !=
                Wasm::Common::JsonParserResultDetail::OK) {
              return false;
            }
            aud_.emplace_back(str_obj_result.first.value());
            return true;
          })) {
    auto aud_field = Wasm::Common::JsonGetField<std::string>(payload_, "aud");
    if (aud_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
      UpdateStatus(Status::JWT_PAYLOAD_PARSE_ERROR);
      return;
    }
    aud_.emplace_back(aud_field.value());
  }

  // Set up signature
  signature_ =
      Base64UrlDecode(std::string(jwt_split[2].begin(), jwt_split[2].end()));
  if (signature_ == "") {
    // Signature is a bad Base64url input.
    UpdateStatus(Status::JWT_SIGNATURE_PARSE_ERROR);
    return;
  }
}

bool Verifier::VerifySignatureRSA(EVP_PKEY *key, const EVP_MD *md,
                                  const uint8_t *signature,
                                  size_t signature_len,
                                  const uint8_t *signed_data,
                                  size_t signed_data_len) {
  bssl::UniquePtr<EVP_MD_CTX> md_ctx(EVP_MD_CTX_create());

  if (EVP_DigestVerifyInit(md_ctx.get(), nullptr, md, nullptr, key) != 1) {
    return false;
  }
  if (EVP_DigestVerifyUpdate(md_ctx.get(), signed_data, signed_data_len) != 1) {
    return false;
  }
  return (EVP_DigestVerifyFinal(md_ctx.get(), signature, signature_len) == 1);
}

bool Verifier::VerifySignatureRSA(EVP_PKEY *key, const EVP_MD *md,
                                  const std::string &signature,
                                  const std::string &signed_data) {
  return VerifySignatureRSA(key, md, CastToUChar(signature), signature.length(),
                            CastToUChar(signed_data), signed_data.length());
}

bool Verifier::VerifySignatureEC(EC_KEY *key, const uint8_t *signature,
                                 size_t signature_len,
                                 const uint8_t *signed_data,
                                 size_t signed_data_len) {
  // ES256 signature should be 64 bytes.
  if (signature_len != 2 * 32) {
    return false;
  }

  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(signed_data, signed_data_len, digest);

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig) {
    UpdateStatus(Status::FAILED_CREATE_ECDSA_SIGNATURE);
    return false;
  }

  BN_bin2bn(signature, 32, ecdsa_sig->r);
  BN_bin2bn(signature + 32, 32, ecdsa_sig->s);
  return (ECDSA_do_verify(digest, SHA256_DIGEST_LENGTH, ecdsa_sig.get(), key) ==
          1);
}

bool Verifier::VerifySignatureEC(EC_KEY *key, const std::string &signature,
                                 const std::string &signed_data) {
  return VerifySignatureEC(key, CastToUChar(signature), signature.length(),
                           CastToUChar(signed_data), signed_data.length());
}

bool Verifier::Verify(const Jwt &jwt, const Pubkeys &pubkeys) {
  // If JWT status is not OK, inherits its status and return false.
  if (jwt.GetStatus() != Status::OK) {
    UpdateStatus(jwt.GetStatus());
    return false;
  }

  // If pubkeys status is not OK, inherits its status and return false.
  if (pubkeys.GetStatus() != Status::OK) {
    UpdateStatus(pubkeys.GetStatus());
    return false;
  }

  std::string signed_data =
      jwt.header_str_base64url_ + '.' + jwt.payload_str_base64url_;
  bool kid_alg_matched = false;
  for (auto &pubkey : pubkeys.keys_) {
    // If kid is specified in JWT, JWK with the same kid is used for
    // verification.
    // If kid is not specified in JWT, try all JWK.
    if (jwt.kid_ != "" && pubkey->kid_specified_ && pubkey->kid_ != jwt.kid_) {
      continue;
    }

    // The same alg must be used.
    if (pubkey->alg_specified_ && pubkey->alg_ != jwt.alg_) {
      continue;
    }
    kid_alg_matched = true;

    if (pubkey->kty_ == "EC" &&
        VerifySignatureEC(pubkey->ec_key_.get(), jwt.signature_, signed_data)) {
      // Verification succeeded.
      return true;
    } else if (pubkey->pem_format_ || pubkey->kty_ == "RSA") {
      const EVP_MD *md;
      if (jwt.alg_ == "RS384") {
        md = EVP_sha384();
      } else if (jwt.alg_ == "RS512") {
        md = EVP_sha512();
      } else {
        // default to SHA256
        md = EVP_sha256();
      }
      if (VerifySignatureRSA(pubkey->evp_pkey_.get(), md, jwt.signature_,
                             signed_data)) {
        // Verification succeeded.
        return true;
      }
    }
  }
  // Verification failed.
  if (kid_alg_matched) {
    UpdateStatus(Status::JWT_INVALID_SIGNATURE);
  } else {
    UpdateStatus(Status::KID_ALG_UNMATCH);
  }
  return false;
}

// Returns the parsed header.
Wasm::Common::JsonObject &Jwt::Header() { return header_; }

const std::string &Jwt::HeaderStr() { return header_str_; }
const std::string &Jwt::HeaderStrBase64Url() { return header_str_base64url_; }
const std::string &Jwt::Alg() { return alg_; }
const std::string &Jwt::Kid() { return kid_; }

// Returns payload JSON.
Wasm::Common::JsonObject &Jwt::Payload() { return payload_; }

const std::string &Jwt::PayloadStr() { return payload_str_; }
const std::string &Jwt::PayloadStrBase64Url() { return payload_str_base64url_; }
const std::string &Jwt::Iss() { return iss_; }
const std::vector<std::string> &Jwt::Aud() { return aud_; }
const std::string &Jwt::Sub() { return sub_; }
int64_t Jwt::Exp() { return exp_; }

void Pubkeys::CreateFromPemCore(const std::string &pkey_pem) {
  keys_.clear();
  std::unique_ptr<Pubkey> key_ptr(new Pubkey());
  EvpPkeyGetter e;
  key_ptr->evp_pkey_ = e.EvpPkeyFromStr(pkey_pem);
  key_ptr->pem_format_ = true;
  UpdateStatus(e.GetStatus());
  if (e.GetStatus() == Status::OK) {
    keys_.push_back(std::move(key_ptr));
  }
}

void Pubkeys::CreateFromJwksCore(const std::string &pkey_jwks) {
  keys_.clear();

  Wasm::Common::JsonObject jwks_json;
  auto result = Wasm::Common::JsonParse(pkey_jwks);
  if (!result.has_value()) {
    UpdateStatus(Status::JWK_PARSE_ERROR);
    return;
  }
  jwks_json = result.value();

  std::vector<std::reference_wrapper<const Wasm::Common::JsonObject>> key_refs;

  if (jwks_json.find("keys") == jwks_json.end()) {
    UpdateStatus(Status::JWK_NO_KEYS);
    return;
  }

  if (!Wasm::Common::JsonArrayIterate(
          jwks_json, "keys", [&](const Wasm::Common::JsonObject &obj) -> bool {
            key_refs.emplace_back(
                std::reference_wrapper<const Wasm::Common::JsonObject>(obj));
            return true;
          })) {
    UpdateStatus(Status::JWK_BAD_KEYS);
    return;
  }

  for (auto &key_ref : key_refs) {
    if (!ExtractPubkeyFromJwk(key_ref.get())) {
      continue;
    }
  }

  if (keys_.size() == 0) {
    UpdateStatus(Status::JWK_NO_VALID_PUBKEY);
  }
}

bool Pubkeys::ExtractPubkeyFromJwk(const Wasm::Common::JsonObject &jwk_json) {
  // Check "kty" parameter, it should exist.
  // https://tools.ietf.org/html/rfc7517#section-4.1
  // If "kty" is missing, getString throws an exception.
  auto kty_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "kty");
  if (kty_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
    return false;
  }

  // Extract public key according to "kty" value.
  // https://tools.ietf.org/html/rfc7518#section-6.1
  if (kty_field.value() == "EC") {
    return ExtractPubkeyFromJwkEC(jwk_json);
  } else if (kty_field.value() == "RSA") {
    return ExtractPubkeyFromJwkRSA(jwk_json);
  }

  return false;
}

bool Pubkeys::ExtractPubkeyFromJwkRSA(
    const Wasm::Common::JsonObject &jwk_json) {
  std::unique_ptr<Pubkey> pubkey(new Pubkey());
  std::string n_str, e_str;

  // "kid" and "alg" are optional, if they do not exist, set them to "".
  // https://tools.ietf.org/html/rfc7517#page-8
  auto kid_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "kid");
  if (kid_field.detail() == Wasm::Common::JsonParserResultDetail::OK) {
    pubkey->kid_ = kid_field.value();
    pubkey->kid_specified_ = true;
  }

  auto alg_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "alg");
  if (alg_field.detail() == Wasm::Common::JsonParserResultDetail::OK) {
    // Allow only "RS" prefixed algorithms.
    // https://tools.ietf.org/html/rfc7518#section-3.1
    if (!(alg_field.value() == "RS256" || alg_field.value() == "RS384" ||
          alg_field.value() == "RS512")) {
      return false;
    }
    pubkey->alg_ = alg_field.value();
    pubkey->alg_specified_ = true;
  }

  auto pubkey_kty_field =
      Wasm::Common::JsonGetField<std::string>(jwk_json, "kty");
  assert(pubkey_kty_field.detail() == Wasm::Common::JsonParserResultDetail::OK);
  pubkey->kty_ = pubkey_kty_field.value();
  auto n_str_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "n");
  auto e_str_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "e");
  if (n_str_field.detail() != Wasm::Common::JsonParserResultDetail::OK ||
      e_str_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
    return false;
  }
  n_str = n_str_field.value();
  e_str = e_str_field.value();

  EvpPkeyGetter e;
  pubkey->evp_pkey_ = e.EvpPkeyFromJwkRSA(n_str, e_str);
  if (e.GetStatus() == Status::OK) {
    keys_.push_back(std::move(pubkey));
  } else {
    UpdateStatus(e.GetStatus());
  }

  return true;
}

bool Pubkeys::ExtractPubkeyFromJwkEC(const Wasm::Common::JsonObject &jwk_json) {
  std::unique_ptr<Pubkey> pubkey(new Pubkey());
  std::string x_str, y_str;

  // "kid" and "alg" are optional, if they do not exist, set them to "".
  // https://tools.ietf.org/html/rfc7517#page-8
  auto kid_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "kid");
  if (kid_field.detail() == Wasm::Common::JsonParserResultDetail::OK) {
    pubkey->kid_ = kid_field.value();
    pubkey->kid_specified_ = true;
  }

  auto alg_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "alg");
  if (alg_field.detail() == Wasm::Common::JsonParserResultDetail::OK) {
    // Allow only "RS" prefixed algorithms.
    // https://tools.ietf.org/html/rfc7518#section-3.1
    if (alg_field.value() != "ES256") {
      return false;
    }
    pubkey->alg_ = alg_field.value();
    pubkey->alg_specified_ = true;
  }

  auto pubkey_kty_field =
      Wasm::Common::JsonGetField<std::string>(jwk_json, "kty");
  assert(pubkey_kty_field.detail() == Wasm::Common::JsonParserResultDetail::OK);
  pubkey->kty_ = pubkey_kty_field.value();
  auto x_str_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "x");
  auto y_str_field = Wasm::Common::JsonGetField<std::string>(jwk_json, "y");
  if (x_str_field.detail() != Wasm::Common::JsonParserResultDetail::OK ||
      y_str_field.detail() != Wasm::Common::JsonParserResultDetail::OK) {
    return false;
  }
  x_str = x_str_field.value();
  y_str = y_str_field.value();

  EvpPkeyGetter e;
  pubkey->ec_key_ = e.EcKeyFromJwkEC(x_str, y_str);
  if (e.GetStatus() == Status::OK) {
    keys_.push_back(std::move(pubkey));
  } else {
    UpdateStatus(e.GetStatus());
  }

  return true;
}

std::unique_ptr<Pubkeys> Pubkeys::CreateFrom(const std::string &pkey,
                                             Type type) {
  std::unique_ptr<Pubkeys> keys(new Pubkeys());
  switch (type) {
    case Type::JWKS:
      keys->CreateFromJwksCore(pkey);
      break;
    case Type::PEM:
      keys->CreateFromPemCore(pkey);
      break;
    default:
      PANIC("can not reach here");
  }
  return keys;
}

}  // namespace JwtAuth
}  // namespace Http
}  // namespace Envoy
