# Copyright 2018 The Bazel Authors. All rights reserved.
#
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# py_test under bazel doesn't appear to auto create the __init__.py files in the
# .runfiles/build_bazel_rules_apple directory; however py_binary does create it.
# The __init__.py files in all the subdirectories along the way are created by
# py_test just fine; but be have to manually create one (and depend on it) so it
# is provided for the py_tests.
