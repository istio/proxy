# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""LinkablePackageInfo module
"""

LinkablePackageInfo = provider(
    doc = "The LinkablePackageInfo provider provides information to the linker for linking pkg_npm built packages",
    fields = {
        "files": "Depset of files in this package (must all be contained within path)",
        "package_name": """The package name.

This field is optional. If not set, the target can be made linkable to a package_name with the npm_link rule.
""",
        "package_path": """The directory in the workspace to link to.

If set, link the 1st party dependencies to the node_modules under the package path specified.
If unset, the default is to link to the node_modules root of the workspace.
""",
        "path": """The path to link to.

Path must be relative to execroot/wksp. It can either an output dir path such as,

`bazel-out/<platform>-<build>/bin/path/to/package` or
`bazel-out/<platform>-<build>/bin/external/external_wksp>/path/to/package`

or a source file path such as,

`path/to/package` or
`external/<external_wksp>/path/to/package`
""",
    },
)
