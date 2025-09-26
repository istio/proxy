include(FetchContent)

# No need to build curl executable
SET(BUILD_CURL_EXE OFF)
SET(BUILD_SHARED_LIBS OFF)
SET(BUILD_STATIC_LIBS ON)

# Disables all protocols except HTTP
SET(HTTP_ONLY ON)
SET(CURL_ENABLE_SSL OFF)
SET(CURL_BROTLI OFF)
SET(CURL_ZSTD OFF)
SET(CURL_ZLIB OFF)
SET(USE_ZLIB OFF)
SET(USE_LIBIDN2 OFF)
SET(CURL_USE_LIBSSH2 OFF)
SET(CURL_USE_LIBPSL OFF)
SET(CURL_DISABLE_HSTS ON)
set(CURL_CA_PATH "none")
set(CURL_CA_PATH_SET FALSE)

FetchContent_Declare(
  curl
  URL "https://github.com/curl/curl/releases/download/curl-7_85_0/curl-7.85.0.tar.gz"
  URL_MD5 "4e9eb4f434e9be889e510f038754d3de"
)

FetchContent_MakeAvailable(curl)
