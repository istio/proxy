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
