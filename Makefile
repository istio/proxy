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

build:
	@bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //...

# Build only envoy - fast
build_envoy:
	bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) //src/envoy/mixer:envoy

clean:
	@bazel clean

test:
	@bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) //...

test_envoy:
	@bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_TEST_ARGS) //src/envoy/mixer/...

check:
	@script/check-license-headers
	@script/check-style

artifacts: build
	@script/push-debian.sh -c opt -p $(ARTIFACTS_DIR)

deb:
	bazel build tools/deb:istio-proxy  ${BAZEL_BUILD_ARGS}

../.repo/manifest.xml:
	(cd ..; echo y | repo init -u http://github.com/costinm/istio-repo)

# Update Istio-proxy dependencies, using 'repo' tool and manifest
repo-sync: ../.repo/manifest.xml
	repo sync -c

cmake-x86:
	mkdir -p ../cmake-build-debug
	(cd ../cmake-build-debug; cmake ..)
	(cd ../cmake-build-debug; make envoy ${CMAKE_MAKE_OPT})

pi:
	mkdir -p ../cmake-pi-debug
	(cd ../cmake-pi-debug; cmake .. -DCMAKE_TOOLCHAIN_FILE=../build/contrib/cmake/pi.toolchain.cmake )
	(cd ../cmake-pi-debug; make envoy ${CMAKE_MAKE_OPT})

.PHONY: build clean test check artifacts repo-sync pi
