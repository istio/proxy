#include <openssl/ssl.h>
#include <ossl.h>

// Dummy buffer just to return a non null value in SSL_get0_peer_certificates
STACK_OF(CRYPTO_BUFFER) *criptoBuffer = NULL;

const STACK_OF(CRYPTO_BUFFER) *SSL_get0_peer_certificates(const SSL *ssl) {
  STACK_OF(X509) *x509Temp = SSL_get_peer_certificate(ssl);
  if(x509Temp == NULL)
    return NULL;
  else {
    if(criptoBuffer == NULL) {
      criptoBuffer = sk_CRYPTO_BUFFER_new_null();
    }
    return criptoBuffer;
  }
}
