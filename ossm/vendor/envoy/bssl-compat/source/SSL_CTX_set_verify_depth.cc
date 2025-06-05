#include <openssl/ssl.h>
#include <ossl.h>


// NOTE: OpenSSL interprets the verify depth differently to BoringSSL. BoringSSL excludes the leaf
//       cert from the verify depth calculation, whereas OpenSSL excludes the leaf AND root cert
//       from the verify depth calculation. Therefore, when passing the depth parameter to OpenSSL
//       we need to subtract 1 from it. See the following 2 links for relevant documentation:
//
// https://www.openssl.org/docs/man3.0/man3/SSL_CTX_set_verify_depth.html
// https://github.com/google/boringssl/blob/ca1690e221677cea3fb946f324eb89d846ec53f2/include/openssl/ssl.h#L2493-L2496

void SSL_CTX_set_verify_depth(SSL_CTX *ctx, int depth) {
  return ossl.ossl_SSL_CTX_set_verify_depth(ctx, depth - 1);
}
