/*
 * OpenSSL 3.0 source has the SHA256 functions marked as deprecated.
 *
 * OSSL_DEPRECATEDIN_3_0 int SHA256_Init(SHA256_CTX *c);
 * OSSL_DEPRECATEDIN_3_0 int SHA256_Update(SHA256_CTX *c,
 *                                         const void *data, size_t len);
 * OSSL_DEPRECATEDIN_3_0 int SHA256_Final(unsigned char *md, SHA256_CTX *c);
 * OSSL_DEPRECATEDIN_3_0 void SHA256_Transform(SHA256_CTX *c,
 *                                             const unsigned char *data);
 *
 * Explicitly mapping functions here to ensure that any move to OpenSSL 3.1
 * and potential BoringSSL divergence from OpenSSL of these functions is noted.
 */
#include <openssl/sha.h>
#include <ossl.h>

// SHA256_Init initialises |sha| and returns 1.
extern "C" {
int SHA256_Init(SHA256_CTX *sha) {
  // BoringSSL and OpenSSL have same success return value
  return ossl.ossl_SHA256_Init(sha);
}

// SHA256_Update adds |len| bytes from |data| to |sha| and returns 1.
int SHA256_Update(SHA256_CTX *sha, const void *data, size_t len) {
    return ossl.ossl_SHA256_Update(sha, data, len);
}

// SHA256_Final adds the final padding to |sha| and writes the resulting digest
// to |out|, which must have at least |SHA256_DIGEST_LENGTH| bytes of space. It
// returns one on success and zero on programmer error.
int SHA256_Final(uint8_t out[SHA256_DIGEST_LENGTH],
                                SHA256_CTX *sha) {
    return ossl.ossl_SHA256_Final(reinterpret_cast<unsigned char *>(out), sha);
}

}
