# Copyright 2023 The Bazel Authors. All rights reserved.
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

# gazelle:ignore abcdefg1,abcdefg2
# gazelle:ignore abcdefg3

import abcdefg1
import abcdefg2
import abcdefg3
import foo

_ = abcdefg1
_ = abcdefg2
_ = abcdefg3
_ = foo

try:
    # gazelle:ignore grpc
    import grpc

    grpc_available = True
except ImportError:
    grpc_available = False

_ = grpc
