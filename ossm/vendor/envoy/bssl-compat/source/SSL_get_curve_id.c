#include <openssl/ssl.h>
#include <ossl.h>
#include "log.h"


/*
 * https://github.com/google/boringssl/blob/098695591f3a2665fccef83a3732ecfc99acdcdd/src/include/openssl/ssl.h#L2345
 * https://www.openssl.org/docs/man3.0/man3/SSL_get_negotiated_group.html
 */
uint16_t SSL_get_curve_id(const SSL *ssl) {
  int nid = ossl.ossl_SSL_get_negotiated_group((SSL*)ssl);
  
  switch(nid) {
    case ossl_NID_secp224r1:        return SSL_CURVE_SECP224R1;
    case ossl_NID_secp384r1:        return SSL_CURVE_SECP384R1;
    case ossl_NID_secp521r1:        return SSL_CURVE_SECP521R1;
    case ossl_NID_X25519:           return SSL_CURVE_X25519;
    case ossl_NID_X9_62_prime256v1: return SSL_CURVE_SECP256R1;
    default: {
      if (nid | ossl_TLSEXT_nid_unknown) {
        return 0;
      }
      bssl_compat_error("Unknown negotiated group nid : %d", nid);
      return 0;
    }
  }
}