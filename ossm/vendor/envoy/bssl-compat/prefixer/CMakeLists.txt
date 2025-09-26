project(prefixer)

# https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project

find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_INSTALL_PREFIX},${LLVM_ENABLE_EH},${LLVM_ENABLE_RTTI},${LLVM_LINK_LLVM_DYLIB})")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM) # For llvm_update_compile_flags()

add_executable(prefixer prefixer.cpp)
llvm_update_compile_flags(prefixer) # Adds appropriate exception & rtti flags
llvm_setup_rpath(prefixer)
target_compile_definitions(prefixer PRIVATE LLVM_LIBRARY_DIR=\"${LLVM_LIBRARY_DIR}\")
target_include_directories(prefixer PRIVATE "${LLVM_INCLUDE_DIRS}")
target_link_directories(prefixer PRIVATE "${LLVM_LIBRARY_DIRS}")
target_link_libraries(prefixer PRIVATE clang-cpp $<$<BOOL:${LLVM_LINK_LLVM_DYLIB}>:LLVM>)
target_link_libraries(prefixer PRIVATE stdc++ stdc++fs)
