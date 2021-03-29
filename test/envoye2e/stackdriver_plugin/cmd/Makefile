# Copyright 2019 Istio Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

  
.PHONY: build_and_push clean all

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
SD_PATH := $(dir $(MKFILE_PATH))
IMG := gcr.io/istio-testing/fake-stackdriver
TAG := 7.0

all: build_and_push clean

build_and_push:
	cd $(SD_PATH) && GOOS=linux GOARCH=amd64 go build main.go
	docker build $(SD_PATH) -t $(IMG):$(TAG)
	docker push $(IMG):$(TAG)
	rm $(SD_PATH)/main
