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

namespace Base64url {

std::string decode(std::string input);

}  // Base64url
//
// namespace Util {
//
// uint8_t* unsigned_c_str(const std::string& str);
// std::vector<std::string> split(std::string str, char c);
//
//}  // Util

class Jwt {
 public:
  std::unique_ptr<rapidjson::Document> decode(const std::string& jwt,
                                              const std::string& pkey_pem);

 private:
  bssl::UniquePtr<EVP_PKEY> evpPkeyFromStr(const std::string& pkey_pem);
  const EVP_MD* evpMdFromAlg(const std::string& alg);
  //  bssl::UniquePtr<const EVP_MD> evp_md_from_alg(const std::string& alg);
  bool verifySignature(bssl::UniquePtr<EVP_PKEY> key, const std::string& alg,
                       const uint8_t* signature, size_t signature_len,
                       const uint8_t* signed_data, size_t signed_data_len);
  bool verifySignature(const std::string& pkey_pem, const std::string& alg,
                       const std::string& signature,
                       const std::string& signed_data);
};

}  // Auth
}  // Http
}  // Envoy

#endif  // PROXY_JWT_H
