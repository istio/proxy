#include <openssl/ssl.h>
#include <ossl.h>

static const size_t kMaxSignatureAlgorithmNameLen = 24;

struct SignatureAlgorithmName {
  uint16_t signature_algorithm;
  const char name[kMaxSignatureAlgorithmNameLen];
};

static const struct SignatureAlgorithmName kSignatureAlgorithmNames[] = {
    {SSL_SIGN_RSA_PKCS1_MD5_SHA1, "rsa_pkcs1_md5_sha1"},
    {SSL_SIGN_RSA_PKCS1_SHA1, "rsa_pkcs1_sha1"},
    {SSL_SIGN_RSA_PKCS1_SHA256, "rsa_pkcs1_sha256"},
    {SSL_SIGN_RSA_PKCS1_SHA256_LEGACY, "rsa_pkcs1_sha256_legacy"},
    {SSL_SIGN_RSA_PKCS1_SHA384, "rsa_pkcs1_sha384"},
    {SSL_SIGN_RSA_PKCS1_SHA512, "rsa_pkcs1_sha512"},
    {SSL_SIGN_ECDSA_SHA1, "ecdsa_sha1"},
    {SSL_SIGN_ECDSA_SECP256R1_SHA256, "ecdsa_secp256r1_sha256"},
    {SSL_SIGN_ECDSA_SECP384R1_SHA384, "ecdsa_secp384r1_sha384"},
    {SSL_SIGN_ECDSA_SECP521R1_SHA512, "ecdsa_secp521r1_sha512"},
    {SSL_SIGN_RSA_PSS_RSAE_SHA256, "rsa_pss_rsae_sha256"},
    {SSL_SIGN_RSA_PSS_RSAE_SHA384, "rsa_pss_rsae_sha384"},
    {SSL_SIGN_RSA_PSS_RSAE_SHA512, "rsa_pss_rsae_sha512"},
    {SSL_SIGN_ED25519, "ed25519"},
};


size_t SSL_get_all_signature_algorithm_names(const char **out, size_t max_out) {
  const char *kPredefinedNames[] = {"ecdsa_sha256", "ecdsa_sha384",
                                    "ecdsa_sha512"};
   size_t predefinedSize = (sizeof(kPredefinedNames) / sizeof(kPredefinedNames[0]));
   size_t nameSize = (sizeof(kSignatureAlgorithmNames) / sizeof(kSignatureAlgorithmNames[0]));
   if(max_out != 0) {
     for(int i = 0; i < predefinedSize; i++) {
        *out++ = kPredefinedNames[i];
     }
     for(int i = 0; i < nameSize; i++) {
        *out++ = kSignatureAlgorithmNames[i].name;
     }
   }
   return predefinedSize+nameSize;
}


