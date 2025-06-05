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

"""Constants used by parts of pip_repository for naming libraries and wheels."""

WHEEL_FILE_PUBLIC_LABEL = "whl"
WHEEL_FILE_IMPL_LABEL = "_whl"
PY_LIBRARY_PUBLIC_LABEL = "pkg"
PY_LIBRARY_IMPL_LABEL = "_pkg"
DATA_LABEL = "data"
DIST_INFO_LABEL = "dist_info"
WHEEL_ENTRY_POINT_PREFIX = "rules_python_wheel_entry_point"
NODEPS_LABEL = "no_deps"
