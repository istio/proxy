find_package(OpenSSL 3.0 COMPONENTS Crypto SSL)

if(OpenSSL_FOUND)
    add_custom_target(OpenSSL)
    get_filename_component(OPENSSL_LIBRARY_DIR ${OPENSSL_CRYPTO_LIBRARY} DIRECTORY)
    message(STATUS "Found OpenSSL ${OPENSSL_VERSION} (${OPENSSL_LIBRARY_DIR})")
else()
    message(STATUS "Building OpenSSL (${OPENSSL_URL})")
    include(ExternalProject)
    set(OPENSSL_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl/source)
    set(OPENSSL_CONFIG_CMD ${OPENSSL_SOURCE_DIR}/config)
    set(OPENSSL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/openssl/install)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INSTALL_DIR}/include)
    set(OPENSSL_LIBRARY_DIR ${OPENSSL_INSTALL_DIR}/lib)
    ExternalProject_Add(OpenSSL
        URL ${OPENSSL_URL}
        URL_HASH SHA256=${OPENSSL_URL_HASH}
        SOURCE_DIR ${OPENSSL_SOURCE_DIR}
        CONFIGURE_COMMAND ${OPENSSL_CONFIG_CMD} --prefix=${OPENSSL_INSTALL_DIR} --libdir=lib
        TEST_COMMAND ""
        INSTALL_COMMAND make install_sw
    )
endif()
