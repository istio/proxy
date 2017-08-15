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

enum class Status {
  OK,

  // Given JWT is not in the form of Header.Payload.Signature
  JWT_BAD_FORMAT,

  // Header is an invalid Base64url input or an invalid JSON.
  JWT_HEADER_PARSE_ERROR,

  // Header does not have "alg".
  JWT_HEADER_NO_ALG,

  // "alg" in the header is not a string.
  JWT_HEADER_BAD_ALG,

  // Signature is an invalid Base64url input.
  JWT_SIGNATURE_PARSE_ERROR,

  // Signature Verification failed (= Failed in DigestVerifyFinal())
  JWT_INVALID_SIGNATURE,

  // Signature is valid but payload is an invalid Base64url input or an invalid
  // JSON.
  JWT_PAYLOAD_PARSE_ERROR,

  // "kid" in the JWT header is not a string.
  JWT_HEADER_BAD_KID,

  // JWK is an invalid JSON.
  JWK_PARSE_ERROR,

  // JWK does not have "keys".
  JWK_NO_KEYS,

  // "keys" in JWK is not an array.
  JWK_BAD_KEYS,

  // There are no valid public key in given JWKs.
  JWK_NO_VALID_PUBKEY,

  // There is no key the kid of which matches that of the given JWT.
  KID_UNMATCH,

  // Value of "alg" in the header is invalid.
  ALG_NOT_IMPLEMENTED,

  // Public key is an invalid Base64 input.
  PUBKEY_PEM_BAD_FORMAT,

  // RSA object was null while creating EVP_PKEY object.
  PUBKEY_RSA_OBJECT_NULL,

  // Failed to create EVP_MD_CTX object.
  EVP_MD_CTX_CREATE_FAIL,

  // Failed in DigestVerifyInit()
  DIGEST_VERIFY_INIT_FAIL,

  // Failed in DigestVerifyUpdate()
  DIGEST_VERIFY_UPDATE_FAIL,
};

std::string StatusToString(Status status);

// Base class for JWT Verifiers.
class JwtVerifier {
 public:
  JwtVerifier() : status_(Status::OK) {}
  virtual ~JwtVerifier() {}

  // This function should be called before Decode().
  virtual JwtVerifier& SetPublicKey(const std::string& pkey) = 0;

  // This function verifies JWT signature and returns the decoded payload as a
  // JSON if the signature is valid.
  // If verification failed, it returns nullptr, and status_ holds the failture
  // reason.
  virtual std::unique_ptr<rapidjson::Document> Decode(
      const std::string& jwt) = 0;
  Status status_;

 protected:
  void UpdateStatus(Status status);
};

// JWT verifier with PEM format public key.
//
// Usage example:
//   JwtVerifierPem v;
//   auto payload = v.SetPublicKey(public_key).Decode(jwt);
class JwtVerifierPem : public JwtVerifier {
 public:
  JwtVerifierPem& SetPublicKey(const std::string& pkey_pem) override;
  std::unique_ptr<rapidjson::Document> Decode(const std::string& jwt) override;

 private:
  bssl::UniquePtr<EVP_PKEY> pkey_;
};

// JWT verifier with JWKs format public keys.
//
// Usage example:
//   JwtVerifierJwks v;
//   auto payload = v.SetPublicKey(public_key).Decode(jwt);
class JwtVerifierJwks : public JwtVerifier {
 public:
  JwtVerifierJwks& SetPublicKey(const std::string& pkey_jwks) override;
  std::unique_ptr<rapidjson::Document> Decode(const std::string& jwt) override;

 private:
  class Jwk {
   public:
    std::string kid_;
    std::string alg_;
    bssl::UniquePtr<EVP_PKEY> pkey_;
    Jwk(){};
  };
  std::vector<std::unique_ptr<Jwk> > jwks_;
};

}  // Auth
}  // Http
}  // Envoy

#endif  // PROXY_JWT_H
