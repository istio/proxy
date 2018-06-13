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
LOCAL_ARTIFACTS_DIR ?= $(abspath artifacts)
ARTIFACTS_DIR ?= $(LOCAL_ARTIFACTS_DIR)
BAZEL_STARTUP_ARGS ?=
BAZEL_BUILD_ARGS ?=
BAZEL_TEST_ARGS ?=
HUB ?=
TAG ?=
ifeq "$(origin CC)" "default"
CC := clang-6.0
endif
ifeq "$(origin CXX)" "default"
CXX := clang++-6.0
endif

build:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //...
	@bazel shutdown

# Build only envoy - fast
build_envoy:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //src/envoy:envoy
	@bazel shutdown

clean:
	@bazel clean
	@bazel shutdown

test:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) //...
	@bazel shutdown

test_asan:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-asan //...
	@bazel shutdown

test_tsan:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-tsan //...
	@bazel shutdown

check:
	@script/check-license-headers
	@script/check-style

artifacts: build
	@script/push-debian.sh -c opt -p $(ARTIFACTS_DIR)

deb:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //tools/deb:istio-proxy
	@bazel shutdown


.PHONY: build clean test check artifacts
