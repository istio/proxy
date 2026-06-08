ifdef NO_CONTEXT
  CPP_CONTEXT_LIB =
else
  CPP_CONTEXT_LIB = ${PROXY_WASM_CPP_SDK}/proxy_wasm_intrinsics.cc
endif

PROTOBUF ?= none
ifeq ($(PROTOBUF), full)
  PROTO_DEPS := protobuf
  PROTO_OPTS := -DPROXY_WASM_PROTOBUF_FULL=1 \
		${PROXY_WASM_CPP_SDK}/proxy_wasm_intrinsics.pb.cc
else ifeq ($(PROTOBUF), lite)
  PROTO_DEPS := protobuf-lite
  PROTO_OPTS := -DPROXY_WASM_PROTOBUF_LITE=1 \
		${PROXY_WASM_CPP_SDK}/proxy_wasm_intrinsics_lite.pb.cc \
		${PROXY_WASM_CPP_SDK}/struct_lite.pb.cc
else
  PROTO_DEPS :=
  PROTO_OPTS :=
endif

# Provide a list of libraries that the wasm depends on (absl_*, re2, etc).
WASM_DEPS ?=

# Determine dependency link options.
# NOTE: Strip out -pthread which RE2 claims to need...
PKG_CONFIG ?= pkg-config
PKG_CONFIG_PATH = ${EMSDK}/upstream/emscripten/cache/sysroot/lib/pkgconfig
WASM_LIBS = $(shell $(PKG_CONFIG) $(WASM_DEPS) $(PROTO_DEPS) \
	  --with-path=$(PKG_CONFIG_PATH) --libs | sed -e 's/-pthread //g')

# See proxy_wasm_cc_binary build rule definition in bazel/defs.bzl for
# explanation of emscripten link options.
EMSCRIPTEN_LINK_OPTS := --no-entry \
	--js-library ${PROXY_WASM_CPP_SDK}/proxy_wasm_intrinsics.js \
	-sSTANDALONE_WASM -sEXPORTED_FUNCTIONS=_malloc \
	-sALLOW_MEMORY_GROWTH=1 -sINITIAL_HEAP=64KB


debug-deps:
	# WASM_DEPS : ${WASM_DEPS}
	# WASM_LIBS : ${WASM_LIBS}
	# PROTO_DEPS: ${PROTO_DEPS}
	# PROTO_OPTS: ${PROTO_OPTS}

%.wasm %.wat: %.cc
	em++ --std=c++17 -O3 -flto \
		${EMSCRIPTEN_LINK_OPTS} \
		-I${PROXY_WASM_CPP_SDK} \
		${CPP_CONTEXT_LIB} \
		${PROTO_OPTS} \
		${WASM_LIBS} \
		$*.cc -o $*.wasm

clean:
	rm *.wasm

# NOTE: How to regenerate .pb.h and .pb.cc files for a protobuf update:
# - download + extract protobuf release (currently v26.1)
# - regenerate:
#   ./bin/protoc --cpp_out=../ -I../ -Iinclude/google/protobuf/ ../struct_lite.proto
#   ./bin/protoc --cpp_out=../ -I../ -Iinclude/google/protobuf/ ../proxy_wasm_intrinsics_lite.proto
#   ./bin/protoc --cpp_out=../ -I../ -Iinclude/google/protobuf/ ../proxy_wasm_intrinsics.proto
