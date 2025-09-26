find_package(OpenSSL 3.0 COMPONENTS Crypto SSL)

if(OpenSSL_FOUND)
    add_custom_target(OpenSSL)
    get_filename_component(OPENSSL_LIBRARY_DIR ${OPENSSL_CRYPTO_LIBRARY} DIRECTORY)
    message(STATUS "Found OpenSSL ${OPENSSL_VERSION} (${OPENSSL_LIBRARY_DIR})")
else()
    message(FATAL_ERROR "OpenSSL 3.0 not found. Aborting.")
endif()
