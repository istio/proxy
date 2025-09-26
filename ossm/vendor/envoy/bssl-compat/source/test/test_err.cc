#include <gtest/gtest.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <limits>


TEST(ErrTest, test_ERR_func_error_string) {
  ASSERT_STREQ("OPENSSL_internal", ERR_func_error_string(0));
  ASSERT_STREQ("OPENSSL_internal", ERR_func_error_string(42));
}


TEST(ErrTest, test_ERR_LIB_SSL_ERR_R_MALLOC_FAILURE) {
  char buf[256]{};

  ERR_clear_error();
  
  ERR_put_error(ERR_LIB_SSL, 0, ERR_R_MALLOC_FAILURE, __FILE__, __LINE__);

  uint32_t e = ERR_get_error();

  EXPECT_EQ(0x10000041, e);
  EXPECT_STREQ("SSL routines", ERR_lib_error_string(e));
  EXPECT_STREQ("malloc failure", ERR_reason_error_string(e));
  EXPECT_STREQ("error:10000041:SSL routines:OPENSSL_internal:malloc failure", ERR_error_string_n(e, buf, sizeof(buf)));
}


/**
 * This covers a fix for test IpVersionsClientVersions/SslCertficateIntegrationTest.ServerEcdsaClientRsaOnlyWithAccessLog/IPv4_TLSv1_3
 * which fails because of an error string mismatch between BoringSSL's string and OpenSSL's string:
 *
 * Expected: "DOWNSTREAM_TRANSPORT_FAILURE_REASON=TLS_error:_268435709:SSL_routines:OPENSSL_internal:NO_COMMON_SIGNATURE_ALGORITHMS"
 * Actual:   "DOWNSTREAM_TRANSPORT_FAILURE_REASON=TLS_error:_167772278:SSL_routines:OPENSSL_internal:no_suitable_signature_algorithm FILTER_CHAIN_NAME=-"
 */
TEST(ErrTest, test_SSL_R_NO_SUITABLE_SIGNATURE_ALGORITHM) {
  char buf[256]{};
  ERR_clear_error();

#ifdef BSSL_COMPAT
  ERR_put_error(ERR_LIB_SSL, 0, ossl_SSL_R_NO_SUITABLE_SIGNATURE_ALGORITHM, __FILE__, __LINE__);
#else // BoringSSL
  ERR_put_error(ERR_LIB_SSL, 0, SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS, __FILE__, __LINE__);
#endif

  uint32_t e = ERR_get_error();

  EXPECT_EQ(268435709, e);
  EXPECT_STREQ("SSL routines", ERR_lib_error_string(e));
  EXPECT_STREQ("NO_COMMON_SIGNATURE_ALGORITHMS", ERR_reason_error_string(e));
  EXPECT_STREQ("error:100000fd:SSL routines:OPENSSL_internal:NO_COMMON_SIGNATURE_ALGORITHMS", ERR_error_string_n(e, buf, sizeof(buf)));
}
