DOCKER_SDK=/sdk
CPP_API:=${DOCKER_SDK}
CPP_CONTEXT_LIB = ${CPP_API}/proxy_wasm_intrinsics.cc
ABSL = /root/abseil-cpp
ABSL_CPP = ${ABSL}/absl/strings/str_cat.cc ${ABSL}/absl/strings/str_split.cc ${ABSL}/absl/strings/numbers.cc ${ABSL}/absl/strings/ascii.cc

PROTO_SRCS = config.pb.cc
COMMON_SRCS = 

all: plugin.wasm

%.wasm %.wat: %.cc ${CPP_API}/proxy_wasm_intrinsics.h ${CPP_API}/proxy_wasm_enums.h ${CPP_API}/proxy_wasm_externs.h ${CPP_API}/proxy_wasm_api.h ${CPP_API}/proxy_wasm_intrinsics.js ${CPP_CONTEXT_LIB}
	protoc config.proto --cpp_out=.
	em++ -s STANDALONE_WASM=1 -s EMIT_EMSCRIPTEN_METADATA=1 -s EXPORTED_FUNCTIONS=['_malloc'] --std=c++17 -O3 -I${CPP_API} -I${CPP_API}/google/protobuf -Iextensions/common -I. -I/usr/local/include -I${ABSL} -I. --js-library ${CPP_API}/proxy_wasm_intrinsics.js $*.cc ${CPP_API}/proxy_wasm_intrinsics.pb.cc ${PROTO_SRCS} ${COMMON_SRCS} ${CPP_CONTEXT_LIB} ${ABSL_CPP} ${CPP_API}/libprotobuf.a -o $*.wasm
	rm -f $*.wast
	rm -f config.pb.*
	chown ${uid}.${gid} $^
