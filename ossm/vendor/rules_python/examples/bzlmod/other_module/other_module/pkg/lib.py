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

from python.runfiles import runfiles


def GetRunfilePathWithCurrentRepository():
    r = runfiles.Create()
    own_repo = r.CurrentRepository()
    # For a non-main repository, the name of the runfiles directory is equal to
    # the canonical repository name.
    return r.Rlocation(own_repo + "/other_module/pkg/data/data.txt")


def GetRunfilePathWithRepoMapping():
    return runfiles.Create().Rlocation("other_module/other_module/pkg/data/data.txt")
