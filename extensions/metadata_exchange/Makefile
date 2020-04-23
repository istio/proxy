DOCKER_SDK=/sdk
CPP_API:=${DOCKER_SDK}
CPP_CONTEXT_LIB = ${CPP_API}/proxy_wasm_intrinsics.cc
ABSL = /root/abseil-cpp
ABSL_CPP = ${ABSL}/absl/strings/str_cat.cc ${ABSL}/absl/strings/str_split.cc ${ABSL}/absl/strings/numbers.cc ${ABSL}/absl/strings/ascii.cc

PROTO_SRCS = config.pb.cc declare_property.pb.cc
COMMON_SRCS = extensions/common/context.cc

all: plugin.wasm

%.wasm %.wat: %.cc ${CPP_API}/proxy_wasm_intrinsics.h ${CPP_API}/proxy_wasm_enums.h ${CPP_API}/proxy_wasm_externs.h ${CPP_API}/proxy_wasm_api.h ${CPP_API}/proxy_wasm_intrinsics.js ${CPP_CONTEXT_LIB}
	protoc config.proto --cpp_out=.
	protoc declare_property.proto --cpp_out=.
	em++ -s STANDALONE_WASM=1 -s EMIT_EMSCRIPTEN_METADATA=1 -s EXPORTED_FUNCTIONS=['_malloc'] --std=c++17 -O3 -I${CPP_API} -I${CPP_API}/google/protobuf -Iextensions/common -I. -I/usr/local/include -I${ABSL} --js-library ${CPP_API}/proxy_wasm_intrinsics.js ${ABSL_CPP} $*.cc ${CPP_API}/proxy_wasm_intrinsics.pb.cc ${PROTO_SRCS} ${COMMON_SRCS} ${CPP_CONTEXT_LIB} ${CPP_API}/libprotobuf.a -o $*.wasm
	rm -f $*.wast
	rm -f extensions/metadata_exchange/config.pb.*
	rm -f extensions/metadata_exchange/declare_property.pb.*
	chown ${uid}.${gid} $^
