DOCKER_SDK=/sdk
CPP_API:=${DOCKER_SDK}
CPP_CONTEXT_LIB = ${CPP_API}/proxy_wasm_intrinsics.cc
ABSL = /root/abseil-cpp
# The linker needs 2 copies to resolve dependencies.
ABSL_LIBS = ${ABSL}/absl/*/*.a ${ABSL}/absl/*/*.a
PROTO_SRCS = extensions/common/node_info.pb.cc config.pb.cc
COMMON_SRCS = extensions/common/context.cc

all: plugin.wasm

%.wasm %.wat: %.cc ${CPP_API}/proxy_wasm_intrinsics.h ${CPP_API}/proxy_wasm_enums.h ${CPP_API}/proxy_wasm_externs.h ${CPP_API}/proxy_wasm_api.h ${CPP_API}/proxy_wasm_intrinsics.js ${CPP_CONTEXT_LIB}
	protoc extensions/common/node_info.proto --cpp_out=.
	protoc config.proto --cpp_out=.
	em++ -s WASM=1 -s BINARYEN_TRAP_MODE='clamp' -s LEGALIZE_JS_FFI=0 -s EMIT_EMSCRIPTEN_METADATA=1 --std=c++17 -O3 -g3 -I${CPP_API} -I${CPP_API}/google/protobuf -Iextensions/common -I. -I/usr/local/include -I${ABSL} -I. --js-library ${CPP_API}/proxy_wasm_intrinsics.js $*.cc ${CPP_API}/proxy_wasm_intrinsics.pb.cc ${PROTO_SRCS} ${COMMON_SRCS} ${CPP_CONTEXT_LIB} ${ABSL_LIBS} ${CPP_API}/libprotobuf.bc -o $*.js
	rm -f $*.js $*.wast
	rm -f extensions/common/node_info.pb.* extensions/stats/config.pb.*
