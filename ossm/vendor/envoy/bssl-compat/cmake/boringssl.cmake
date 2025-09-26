if(BUILD_BORINGSSL)
  include(ExternalProject)

  ExternalProject_Add(BoringSSL
    PREFIX      "${CMAKE_CURRENT_BINARY_DIR}/third_party/boringssl/src"
    SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boringssl/src"
    CMAKE_ARGS  -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
                -DCMAKE_INSTALL_LIBDIR=lib
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  )

  ExternalProject_Get_Property(BoringSSL INSTALL_DIR)
  file(MAKE_DIRECTORY ${INSTALL_DIR}/include)

  add_library(BoringSSL::SSL STATIC IMPORTED GLOBAL)
  set_property(TARGET BoringSSL::SSL PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libssl.a)
  set_property(TARGET BoringSSL::SSL PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
  add_dependencies(BoringSSL::SSL BoringSSL)

  add_library(BoringSSL::Crypto STATIC IMPORTED GLOBAL)
  set_property(TARGET BoringSSL::Crypto PROPERTY IMPORTED_LOCATION ${INSTALL_DIR}/lib/libcrypto.a)
  set_property(TARGET BoringSSL::Crypto PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include)
  add_dependencies(BoringSSL::Crypto BoringSSL)
endif(BUILD_BORINGSSL)

add_custom_target(bssl-gen)

function(_target_add_bssl_file target src-file dst-file)
  target_sources(${target} PRIVATE ${dst-file})
  set(generate-cmd "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate.h.sh" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${src-file}" "${dst-file}")
  foreach(dependency "third_party/boringssl/src/${src-file}" "patch/${dst-file}.sh" "patch/${dst-file}.patch")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}")
      set(dependencies ${dependencies} "${CMAKE_CURRENT_SOURCE_DIR}/${dependency}")
    endif()
  endforeach()
  set(dependencies ${dependencies} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate.h.sh")
  set(dependencies ${dependencies} "${CMAKE_CURRENT_SOURCE_DIR}/tools/uncomment.sh")
  add_custom_command(COMMAND ${generate-cmd} DEPENDS ${dependencies} OUTPUT "${dst-file}")
  string(MAKE_C_IDENTIFIER "${dst-file}" dst-file-target)
  if(NOT TARGET ${dst-file-target})
    add_custom_target(${dst-file-target} DEPENDS "${dst-file}")
    add_dependencies(bssl-gen ${dst-file-target})
  endif()
endfunction()

function(target_add_bssl_include target)
  foreach(src-file ${ARGN})
    _target_add_bssl_file(${target} "${src-file}" "${src-file}")
  endforeach()
endfunction()

function(target_add_bssl_source target)
  foreach(dst-file ${ARGN})
    cmake_path(RELATIVE_PATH dst-file BASE_DIRECTORY "source" OUTPUT_VARIABLE src-file)
    _target_add_bssl_file(${target} "${src-file}" "${dst-file}")
  endforeach()
endfunction()

function(target_add_bssl_function target)
  set(gen-c-sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate.c.sh)
  foreach(function ${ARGN})
    set(gen-file source/${function}.c)
    set(gen-cmd flock ${gen-c-sh} -c "${gen-c-sh} ${function} ${gen-file}")
    target_sources(${target} PRIVATE ${gen-file})
    add_custom_command(OUTPUT ${gen-file} COMMAND ${gen-cmd} DEPENDS ${gen-c-sh})
  endforeach()
endfunction()

add_custom_command(OUTPUT source/crypto/test/crypto_test_data.cc
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boringssl/crypto_test_data.cc
                   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boringssl/crypto_test_data.cc
                                                    source/crypto/test/crypto_test_data.cc
)