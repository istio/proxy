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
# Don't build Debian packages and Docker images in tests.
BAZEL_TEST_TARGETS ?= ${BAZEL_TARGETS}
E2E_TEST_TARGETS ?= $$(go list ./...)
E2E_TEST_FLAGS := -p=1 -parallel=1
HUB ?=
TAG ?=
repo_dir := .

VERBOSE ?=
ifeq "$(VERBOSE)" "1"
BAZEL_STARTUP_ARGS := --client_debug $(BAZEL_STARTUP_ARGS)
BAZEL_BUILD_ARGS := -s --sandbox_debug --verbose_failures $(BAZEL_BUILD_ARGS)
endif

BAZEL_CONFIG =

UNAME := $(shell uname)
ifeq ($(UNAME),Linux)
BAZEL_CONFIG_DEV  = $(BAZEL_CONFIG)
BAZEL_CONFIG_REL  = $(BAZEL_CONFIG) --config=release
BAZEL_CONFIG_ASAN = $(BAZEL_CONFIG) --config=clang-asan-ci
BAZEL_CONFIG_TSAN = $(BAZEL_CONFIG) --config=clang-tsan-ci
endif
ifeq ($(UNAME),Darwin)
BAZEL_CONFIG_DEV  = # macOS always links against libc++
BAZEL_CONFIG_REL  = --config=release
BAZEL_CONFIG_ASAN = --config=macos-asan
BAZEL_CONFIG_TSAN = # no working config
endif
BAZEL_CONFIG_CURRENT ?= $(BAZEL_CONFIG_DEV)

TEST_ENVOY_TARGET ?= //:envoy
TEST_ENVOY_DEBUG ?= trace

build:
	bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_CURRENT) -- $(BAZEL_TARGETS)

build_envoy: BAZEL_CONFIG_CURRENT = $(BAZEL_CONFIG_REL)
build_envoy: BAZEL_TARGETS = //:envoy
build_envoy: build

build_envoy_tsan: BAZEL_CONFIG_CURRENT = $(BAZEL_CONFIG_TSAN)
build_envoy_tsan: BAZEL_TARGETS = //:envoy
build_envoy_tsan: build

build_envoy_asan: BAZEL_CONFIG_CURRENT = $(BAZEL_CONFIG_ASAN)
build_envoy_asan: BAZEL_TARGETS = //:envoy
build_envoy_asan: build

check_wasm:
	@true

clean:
	@bazel clean

.PHONY: gen-extensions-doc
gen-extensions-doc:
	buf generate --path extensions/

gen:
	@scripts/gen-testdata.sh

gen-check:
	@scripts/gen-testdata.sh -c

test:
	bazel $(BAZEL_STARTUP_ARGS) build $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_CURRENT) -- $(TEST_ENVOY_TARGET) $(BAZEL_TEST_TARGETS)
	if [ -n "$(BAZEL_TEST_TARGETS)" ]; then \
	  bazel $(BAZEL_STARTUP_ARGS) test $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_CURRENT) -- $(BAZEL_TEST_TARGETS); \
	fi
	if [ -n "$(E2E_TEST_TARGETS)" ]; then \
	  env ENVOY_DEBUG=$(TEST_ENVOY_DEBUG) ENVOY_PATH=$(shell bazel info $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_CURRENT) bazel-bin)/envoy $(E2E_TEST_ENVS) GO111MODULE=on go test -timeout 30m $(E2E_TEST_FLAGS) $(E2E_TEST_TARGETS); \
	fi

test_asan: BAZEL_CONFIG_CURRENT = $(BAZEL_CONFIG_ASAN)
test_asan: E2E_TEST_ENVS = ASAN=true
test_asan: test

test_tsan: BAZEL_CONFIG_CURRENT = $(BAZEL_CONFIG_TSAN)
test_tsan: E2E_TEST_ENVS = TSAN=true
test_tsan: TEST_ENVOY_DEBUG = debug # tsan is too slow for trace
test_tsan: test

check:
	@echo >&2 "Please use \"make lint\" instead."
	@false

lint: lint-copyright-banner format-go lint-go tidy-go lint-scripts
	@scripts/check-repository.sh
	@scripts/check-style.sh
	@scripts/verify-last-flag-matches-upstream.sh

protoc = protoc -I common-protos -I extensions
protoc_gen_docs_plugin := --docs_out=camel_case_fields=false,warnings=true,per_file=true,mode=html_fragment_with_front_matter:$(repo_dir)/

metadata_exchange_path := extensions/metadata_exchange
metadata_exchange_protos := $(wildcard $(metadata_exchange_path)/*.proto)
metadata_exchange_docs := $(metadata_exchange_protos:.proto=.pb.html)
$(metadata_exchange_docs): $(metadata_exchange_protos)
	@$(protoc) -I ./extensions $(protoc_gen_docs_plugin)$(metadata_exchange_path) $^

stackdriver_path := extensions/stackdriver/config/v1alpha1
stackdriver_protos := $(wildcard $(stackdriver_path)/*.proto)
stackdriver_docs := $(stackdriver_protos:.proto=.pb.html)
$(stackdriver_docs): $(stackdriver_protos)
	@$(protoc) -I ./extensions $(protoc_gen_docs_plugin)$(stackdriver_path) $^

accesslog_policy_path := extensions/access_log_policy/config/v1alpha1
accesslog_policy_protos := $(wildcard $(accesslog_policy_path)/*.proto)
accesslog_policy_docs := $(accesslog_policy_protos:.proto=.pb.html)
$(accesslog_policy_docs): $(accesslog_policy_protos)
	@$(protoc) -I ./extensions $(protoc_gen_docs_plugin)$(accesslog_policy_path) $^

extensions-docs:  $(metadata_exchange_docs) $(stackdriver_docs) $(accesslog_policy_docs)

test_release:
ifeq "$(shell uname -m)" "x86_64"
	export BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh
else
	# Only x86 has support for legacy GLIBC, otherwise pass -i to skip the check
	export BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh -i
endif

push_release:
ifeq "$(shell uname -m)" "x86_64"
	export BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh -d "$(RELEASE_GCS_PATH)" ${PUSH_RELEASE_FLAGS}
else
	# Only x86 has support for legacy GLIBC, otherwise pass -i to skip the check
	export BAZEL_BUILD_ARGS="$(BAZEL_BUILD_ARGS)" && ./scripts/release-binary.sh -i -d "$(RELEASE_GCS_PATH)" ${PUSH_RELEASE_FLAGS}
endif

# Used by build container to export the build output from the docker volume cache
exportcache: BAZEL_BIN_PATH ?= $(shell bazel info $(BAZEL_BUILD_ARGS) $(BAZEL_CONFIG_CURRENT) bazel-bin)
exportcache:
	@mkdir -p /work/out/$(TARGET_OS)_$(TARGET_ARCH)
	@cp -a $(BAZEL_BIN_PATH)/envoy /work/out/$(TARGET_OS)_$(TARGET_ARCH)
	@chmod +w /work/out/$(TARGET_OS)_$(TARGET_ARCH)/envoy
	@cp -a $(BAZEL_BIN_PATH)/**/*wasm /work/out/$(TARGET_OS)_$(TARGET_ARCH) &> /dev/null || true

.PHONY: build clean test check extensions-proto

include common/Makefile.common.mk
