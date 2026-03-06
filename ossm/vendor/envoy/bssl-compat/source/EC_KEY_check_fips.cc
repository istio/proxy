#include <openssl/ec_key.h>
#include <openssl/evp.h>
#include <ossl.h>


extern "C" int EC_KEY_check_fips(const EC_KEY *key) {
  bssl::UniquePtr<EVP_PKEY> pkey(ossl.ossl_EVP_PKEY_new());
  if (!pkey) {
    return 0;
  }

  if (!ossl.ossl_EVP_PKEY_set1_EC_KEY(pkey.get(), const_cast<EC_KEY*>(key))) {
    return 0;
  }

  bssl::UniquePtr<EVP_PKEY_CTX> ctx(ossl.ossl_EVP_PKEY_CTX_new(pkey.get(), nullptr));
  if (!ctx) {
    return 0;
  }

  // The following check will be routed to the FIPS provider if it's loaded
  return ossl.ossl_EVP_PKEY_pairwise_check(ctx.get()) == 1 ? 1 : 0;
}
