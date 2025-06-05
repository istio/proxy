#include <openssl/rsa.h>
#include <ossl.h>


static int bn_dup_into_key(RSA *rsa, const BIGNUM *n, const BIGNUM *e, const BIGNUM *d) {
  if (n == NULL || e == NULL || d == NULL) {
//    OPENSSL_PUT_ERROR(RSA, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  // BIGNUM rsa_n, rsa_e, rsa_d;
  // ossl.ossl_RSA_get0_key(rsa, &rsa_n, &rsa_e, &rsa_d);
  return(ossl.ossl_RSA_set0_key(rsa, ossl.ossl_BN_dup(n), 
                                     ossl.ossl_BN_dup(e), 
                                     ossl.ossl_BN_dup(d)));
}

static int bn_dup_into_factors(RSA *rsa, const BIGNUM *p, const BIGNUM *q) {
  if (p == NULL || q == NULL) {
//    OPENSSL_PUT_ERROR(RSA, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  return(ossl.ossl_RSA_set0_factors(rsa, ossl.ossl_BN_dup(p), 
                                     ossl.ossl_BN_dup(q)));
}

static int bn_dup_into_crt_params(RSA *rsa, const BIGNUM *dmp1, const BIGNUM *dmq1, const BIGNUM *iqmp) {
  if (dmp1 == NULL || dmq1 == NULL || iqmp == NULL) {
//    OPENSSL_PUT_ERROR(RSA, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  return(ossl.ossl_RSA_set0_crt_params(rsa, ossl.ossl_BN_dup(dmp1),
                                       ossl.ossl_BN_dup(dmq1), 
                                       ossl.ossl_BN_dup(iqmp)));
}

RSA *RSA_new_public_key(const BIGNUM *n, const BIGNUM *e) {
#ifdef FUTURE_CODE
// TODO
  RSA *rsa = ossl.ossl_RSA_new();
  
  if (rsa == NULL ||
      !bn_dup_into_key(rsa, n, e, d) ||
      !RSA_check_key(rsa)) {
    ossl.ossl_RSA_free(rsa);
    return NULL;
  }
  return rsa;
#else
  return NULL;
#endif
}

RSA *RSA_new_private_key(const BIGNUM *n, const BIGNUM *e, const BIGNUM *d,
                         const BIGNUM *p, const BIGNUM *q, const BIGNUM *dmp1,
                         const BIGNUM *dmq1, const BIGNUM *iqmp) {
  RSA *rsa = ossl.ossl_RSA_new();
  if (rsa == NULL ||
      !bn_dup_into_key(rsa, n, e, d) ||
      !bn_dup_into_factors(rsa, p, q) ||
      !bn_dup_into_crt_params(rsa, n, e, d) ||
      !ossl.ossl_RSA_check_key(rsa)) {
    ossl.ossl_RSA_free(rsa);
    return NULL;
  }

  return rsa;
}


RSA *RSA_new_private_key_no_crt(const BIGNUM *n, const BIGNUM *e,
                                const BIGNUM *d) {
  RSA *rsa = ossl.ossl_RSA_new();
  if (rsa == NULL ||               //
      !bn_dup_into_key(rsa, n, e, d) ||
      !RSA_check_key(rsa)) {
    ossl.ossl_RSA_free(rsa);
    return NULL;
  }

  return rsa;
}


RSA *RSA_new_private_key_no_e(const BIGNUM *n, const BIGNUM *d) {
#ifdef FUTURE_CODE
// TODO
 RSA *rsa = ossl.ossl_RSA_new();
  if (rsa == NULL) {
    return NULL;
  }

  rsa->flags |= RSA_FLAG_NO_PUBLIC_EXPONENT;
  if (!bn_dup_into(&rsa->n, n) ||  //
      !bn_dup_into(&rsa->d, d) ||  //
      !ossl.ossl_RSA_check_key(rsa)) {
    ossl.ossl_RSA_free(rsa);
    return NULL;
  }

  return rsa;
#else
  return NULL;
#endif
}
