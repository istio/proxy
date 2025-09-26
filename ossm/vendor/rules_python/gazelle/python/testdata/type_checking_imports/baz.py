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


# While this format is not official, it is supported by most type checkers and
# is used in the wild to avoid importing the typing module.
TYPE_CHECKING = False
if TYPE_CHECKING:
    # Both boto3 and boto3_stubs should be added to pyi_deps.
    import boto3

X = 1
