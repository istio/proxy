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
BAZEL_TARGETS ?= //...
# Some tests run so slowly under the santizers that they always timeout.
SANITIZER_EXCLUSIONS ?= -test/integration:mixer_fault_test
HUB ?=
TAG ?=
ifeq "$(origin CC)" "default"
CC := clang-7
endif
ifeq "$(origin CXX)" "default"
CXX := clang++-7
endif
PATH := /usr/lib/llvm-7/bin:$(PATH)

# Removed 'bazel shutdown' as it could cause CircleCI to hang
build:
	PATH=$(PATH) CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_TARGETS)

# Build only envoy - fast
build_envoy:
	PATH=$(PATH) CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //src/envoy:envoy

clean:
	@bazel clean

test:
	PATH=$(PATH) CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) $(BAZEL_TARGETS)

test_asan:
	PATH=$(PATH) CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-asan -- $(BAZEL_TARGETS) $(SANITIZER_EXCLUSIONS)

test_tsan:
	PATH=$(PATH) CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) --config=clang-tsan --test_env=TSAN_OPTIONS=supressions=$(TOP)/tsan.suppressions -- $(BAZEL_TARGETS) $(SANITIZER_EXCLUSIONS)

check:
	@script/check-license-headers
	@script/check-repositories
	@script/check-style

artifacts: build
	@script/push-debian.sh -c opt -p $(ARTIFACTS_DIR)

deb:
	CC=$(CC) CXX=$(CXX) bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //tools/deb:istio-proxy


.PHONY: build clean test check artifacts
