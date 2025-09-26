#define _GNU_SOURCE
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "ossl_dlopen.h"


/**
 * @brief Dynamically loads OpenSSL shared libraries with environment-specific path resolution.
 *
 * This function is called by ossl_init() to load OpenSSL's libcrypto.so and libssl.so at runtime.
 * It handles two different execution environments:
 *
 * 1. **Bazel build/test environment** (When RUNFILES_DIR & TEST_WORKSPACE are set):
 *    - OpenSSL libraries are built by Bazel and put in the runfiles directory as data dependencies
 *    - Libraries are loaded from: $RUNFILES_DIR/$TEST_WORKSPACE/external/openssl/openssl/lib/
 *    - Ensures the tests always use the correct Bazel-built libs, rather than libs from elsewhere
 *
 * 2. **Production/system environment** (When RUNFILES_DIR & TEST_WORKSPACE are not set):
 *    - Standard dlopen() behavior with LD_LIBRARY_PATH search
 *    - Expects OpenSSL libraries to be available in system paths
 *
 * In both cases, we use RTLD_DEEPBIND to ensure symbols are resolved from the loaded OpenSSL
 * library. Without this, bssl-compat will end up finding its own symbols instead of the loaded
 * OpenSSL ones.
 *
 * @param name The library filename (e.g., "libcrypto.so.3", "libssl.so.3")
 * @return void* Handle to the loaded library, or NULL on failure
 */
void* ossl_dlopen(const char* name) {
  void* handle = NULL;
  const char* runfiles_dir = getenv("RUNFILES_DIR");
  const char* test_workspace = getenv("TEST_WORKSPACE");

  if (runfiles_dir && test_workspace) {
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof(fullpath), "%s/%s/%s/%s",
              runfiles_dir, test_workspace,
              "external/openssl/openssl/lib", name);
    handle = dlopen(fullpath, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
  }
  else {
    handle = dlopen(name, RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
  }

  if(handle && getenv("BSSL_COMPAT_DEBUG_DLOPEN")) {
    char origin[PATH_MAX];
    if (dlinfo(handle, RTLD_DI_ORIGIN, origin) == 0) {
      fprintf(stderr, "bssl-compat: Loaded %s from %s%s\n",
        name, origin, (runfiles_dir ? " (using RUNFILES_DIR)" : ""));
    }
  }

  return handle;
}
