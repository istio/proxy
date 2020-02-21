## Copyright 2017 Istio Authors
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

TOP := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

SHELL := /bin/bash
BAZEL_STARTUP_ARGS ?=
BAZEL_BUILD_ARGS ?=
BAZEL_TARGETS ?= //...
HUB ?=
TAG ?=

ifeq "$(origin CC)" "default"
CC := clang
endif
ifeq "$(origin CXX)" "default"
CXX := clang++
endif
PATH := /usr/lib/llvm-9/bin:$(PATH)

VERBOSE ?=
ifeq "$(VERBOSE)" "1"
BAZEL_STARTUP_ARGS := --client_debug $(BAZEL_STARTUP_ARGS)
BAZEL_BUILD_ARGS := -s --sandbox_debug --verbose_failures $(BAZEL_BUILD_ARGS)
endif

UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
BAZEL_CONFIG_DEV  = --config=libc++
BAZEL_CONFIG_REL  = --config=libc++ --config=release
BAZEL_CONFIG_ASAN = --config=libc++ --config=clang-asan
BAZEL_CONFIG_TSAN = --config=libc++ --config=clang-tsan
endif
ifeq ($(UNAME),Darwin)
BAZEL_CONFIG_DEV  = # macOS always links against libc++
BAZEL_CONFIG_REL  = --config=release
BAZEL_CONFIG_ASAN = --config=macos-asan
BAZEL_CONFIG_TSAN = # no working config
endif

BAZEL_OUTPUT_PATH := $(shell bazel info $(BAZEL_BUILD_ARGS) output_path)
BAZEL_ENVOY_PATH ?= $(BAZEL_OUTPUT_PATH)/k8-fastbuild/bin/src/envoy/envoy

build:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_DEV) $(BAZEL_TARGETS)

build_envoy:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_REL) //src/envoy:envoy

build_wasm:
	$(foreach file, $(shell find extensions -name build_wasm.sh), cd $(TOP)/$(shell dirname $(file)) && bash ./build_wasm.sh &&) true

clean:
	@bazel clean

test:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_DEV) //src/envoy:envoy
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_DEV) $(BAZEL_TARGETS)
	env ENVOY_PATH=$(BAZEL_ENVOY_PATH) GO111MODULE=on go test ./...

test_asan:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_ASAN) //src/envoy:envoy
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_ASAN) -- $(BAZEL_TARGETS)
	env ENVOY_PATH=$(BAZEL_ENVOY_PATH) GO111MODULE=on go test ./...

test_tsan:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_TSAN) //src/envoy:envoy
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_TSAN) -- $(BAZEL_TARGETS)
	env ENVOY_PATH=$(BAZEL_ENVOY_PATH) GO111MODULE=on go test ./...

check:
	@echo >&2 "Please use \"make lint\" instead."
	@false

lint: lint-copyright-banner
	@scripts/check-repository.sh
	@scripts/check-style.sh

deb:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) && bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_REL) //tools/deb:istio-proxy

artifacts:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/push-debian.sh -p "$(ARTIFACTS_GCS_PATH)" -o "$(ARTIFACTS_DIR)"

test_release:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh -i

push_release:
	export PATH=$(PATH) CC=$(CC) CXX=$(CXX) BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh -d "$(RELEASE_GCS_PATH)" -p

.PHONY: build clean test check artifacts

include common/Makefile.common.mk
