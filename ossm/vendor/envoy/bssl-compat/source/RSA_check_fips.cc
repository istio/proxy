#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <ossl.h>


extern "C" int RSA_check_fips(RSA *key) {
  bssl::UniquePtr<EVP_PKEY> pkey(ossl.ossl_EVP_PKEY_new());
  if (!pkey) {
    return 0;
  }

  if (!ossl.ossl_EVP_PKEY_set1_RSA(pkey.get(), key)) {
    return 0;
  }

  bssl::UniquePtr<EVP_PKEY_CTX> ctx(ossl.ossl_EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (!ctx) {
    return 0;
  }

  const BIGNUM *d = nullptr;
  ossl.ossl_RSA_get0_key(key, nullptr, nullptr, &d);

  // The following checks will be routed to the FIPS provider if it's loaded

  if (d) { // Private key
    return ossl.ossl_EVP_PKEY_pairwise_check(ctx.get()) == 1 ? 1 : 0;
  }
  else { // Public key
    return ossl.ossl_EVP_PKEY_public_check(ctx.get()) == 1 ? 1 : 0;
  }
}
