#include <openssl/rsa.h>
#include <ossl.h>
// #include <atomic>

// FIXME: doesn't find the type defined in internal.h
typedef uint32_t CRYPTO_atomic_u32;

// g_buffering_enabled is one if fork-unsafe buffering has been enabled and zero
// otherwise.
static CRYPTO_atomic_u32 g_buffering_enabled = 0;

void RAND_enable_fork_unsafe_buffering(int fd) {
  // We no longer support setting the file-descriptor with this function.
  if (fd != -1) {
    abort();
  }
#ifdef FUTURE_CODE
// FIXME doesn't find the function in internal.h
  CRYPTO_atomic_store_u32(&g_buffering_enabled, 1);
#endif
}
