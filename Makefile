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
BAZEL_BIN ?= bazel
SHELL := /bin/bash
LOCAL_ARTIFACTS_DIR ?= $(abspath artifacts)
ARTIFACTS_DIR ?= $(LOCAL_ARTIFACTS_DIR)
BAZEL_STARTUP_ARGS ?= --batch
BAZEL_BUILD_ARGS ?=
BAZEL_TEST_ARGS ?=
HUB ?=
TAG ?=

build:
	@${BAZEL_BIN} $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //...

# Build only envoy - fast
build_envoy:
	${BAZEL_BIN} $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //src/envoy/tcp/mixer:filter_lib //src/envoy/http/mixer:filter_lib

clean:
	@${BAZEL_BIN} clean

test:
	${BAZEL_BIN} $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) //...

test_asan:
	CC=clang-5.0 CXX=clang++-5.0 ${BAZEL_BIN} $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-asan //...

test_tsan:
	CC=clang-5.0 CXX=clang++-5.0 ${BAZEL_BIN} $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-tsan //...

check:
	@script/check-license-headers
	@script/check-style

artifacts: build
	@script/push-debian.sh -c opt -p $(ARTIFACTS_DIR)

deb:
	@${BAZEL_BIN} build tools/deb:istio-proxy ${BAZEL_BUILD_ARGS}


.PHONY: build clean test check artifacts
