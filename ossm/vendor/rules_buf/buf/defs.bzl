# Copyright 2021-2023 Buf Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
# rules for building protocol buffers using buf

## Overview

The rules work alongside `proto_library` rule. They support,

- Linting ([buf_lint_test](#buf_lint_test))
- Breaking change detection ([buf_breaking_test](#buf_breaking_test)) 

Use [gazelle](/gazelle/buf) to auto generate all of these rules based on `buf.yaml`.

"""

load("//buf/internal:breaking.bzl", _buf_breaking_test = "buf_breaking_test")
load("//buf/internal:lint.bzl", _buf_lint_test = "buf_lint_test")
load("//buf/internal:repo.bzl", _buf_dependencies = "buf_dependencies")

buf_dependencies = _buf_dependencies

def buf_breaking_test(timeout = "short", **kwargs):
    _buf_breaking_test(timeout = timeout, **kwargs)

def buf_lint_test(timeout = "short", **kwargs):
    _buf_lint_test(timeout = timeout, **kwargs)
